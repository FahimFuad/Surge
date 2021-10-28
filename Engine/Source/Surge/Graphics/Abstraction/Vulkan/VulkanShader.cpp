// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanShader.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <shaderc/shaderc.hpp>

#ifdef SURGE_DEBUG
#define SHADER_LOG(...) Log<Severity::Debug>(__VA_ARGS__);
#else
#define SHADER_LOG(...)
#endif

namespace Surge
{
    VulkanShader::VulkanShader(const Path& path)
        : mPath(path)
    {
        ParseShader();
    }

    VulkanShader::~VulkanShader()
    {
        SG_ASSERT(mCallbacks.empty(), "Callbacks must be empty! Did you forgot to call 'RemoveReloadCallback(id);' somewhere?");
        Clear();
        mShaderSources.clear();
    }

    void VulkanShader::Load(const HashMap<ShaderType, bool>& compileStages)
    {
        SCOPED_TIMER("Shader({0}) Compilation", Filesystem::GetNameWithExtension(mPath));
        Clear();
        ParseShader();
        Compile(compileStages);
        CreateVulkanDescriptorSetLayouts();
        CreateVulkanPushConstantRanges();
    }

    void VulkanShader::Reload()
    {
        Load();
        for (const auto& [id, callback] : mCallbacks)
            callback();
    }

    Surge::CallbackID VulkanShader::AddReloadCallback(const std::function<void()> callback)
    {
        UUID id = UUID();
        mCallbacks[id] = callback;
        return id;
    }

    void VulkanShader::RemoveReloadCallback(const CallbackID& id)
    {
        auto itr = mCallbacks.find(id);
        if (itr != mCallbacks.end())
        {
            mCallbacks.erase(id);
            return;
        }
        SG_ASSERT_INTERNAL("Invalid CallbackID!");
    }

    void VulkanShader::Compile(const HashMap<ShaderType, bool>& compileStages)
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        bool saveHash = false;

        // NOTE(Rid - AC3R) If we enable optimization, it removes the name :kekCry:
        // options.SetOptimizationLevel(shaderc_optimization_level_performance);

        for (auto&& [stage, source] : mShaderSources)
        {
            SPIRVHandle spirvHandle;
            spirvHandle.Type = stage;
            bool compile = true; // Default is "true", shader should be compiled if not specified in compileStages

            // If compileStages is not empty, then try to find the "compile" bool
            // This "compile" bool determines if the shader should be recompiled or not
            if (!compileStages.empty())
            {
                auto itr = compileStages.find(stage);
                if (itr != compileStages.end())
                    compile = compileStages.at(stage);
            }

            // Load or create the SPIRV
            if (compile)
            {
                // Compile, not present in cache
                shaderc::CompilationResult result = compiler.CompileGlslToSpv(source, VulkanUtils::ShadercShaderKindFromSurgeShaderType(stage), mPath.c_str(), options);
                if (result.GetCompilationStatus() != shaderc_compilation_status_success)
                {
                    Log<Severity::Error>("{0} Shader compilation failure!", VulkanUtils::ShaderTypeToString(stage));
                    Log<Severity::Error>("{0} Error(s): \n{1}", result.GetNumErrors(), result.GetErrorMessage());
                    SG_ASSERT_INTERNAL("Shader Compilation failure!");
                }
                else
                    spirvHandle.SPIRV = Vector<Uint>(result.cbegin(), result.cend());
            }
            else
            {
                // Load from cache
                String name = fmt::format("{0}.{1}.spv", Filesystem::GetNameWithExtension(mPath), ShaderTypeToString(stage));
                String path = fmt::format("{0}/{1}", SHADER_CACHE_PATH, name);

                FILE* f;
                fopen_s(&f, path.c_str(), "rb");
                if (f)
                {
                    fseek(f, 0, SEEK_END);
                    uint64_t size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    spirvHandle.SPIRV = Vector<Uint>(size / sizeof(Uint));
                    fread(spirvHandle.SPIRV.data(), sizeof(Uint), spirvHandle.SPIRV.size(), f);
                    fclose(f);
                    SHADER_LOG("Loaded Shader from cache: {0}", path);
                }
            }
            SG_ASSERT(!spirvHandle.SPIRV.empty(), "Invalid SPIRV!");

            // Create the VkShaderModule
            VkShaderModuleCreateInfo createInfo {};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = spirvHandle.SPIRV.size() * sizeof(Uint);
            createInfo.pCode = spirvHandle.SPIRV.data();
            VK_CALL(vkCreateShaderModule(device, &createInfo, nullptr, &mVkShaderModules[stage]));
            SET_VK_OBJECT_DEBUGNAME(mVkShaderModules.at(stage), VK_OBJECT_TYPE_SHADER_MODULE, "Vulkan Shader");
            mShaderSPIRVs.push_back(spirvHandle);
        }
        ShaderReflector reflector;
        mReflectionData = reflector.Reflect(mShaderSPIRVs);
    }

    void VulkanShader::Clear()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        mShaderSPIRVs.clear();

        for (auto&& [stage, source] : mVkShaderModules)
        {
            if (mVkShaderModules[stage])
                vkDestroyShaderModule(device, mVkShaderModules[stage], nullptr);
        }

        for (auto& descriptorSetLayout : mDescriptorSetLayouts)
        {
            if (descriptorSetLayout.second)
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout.second, nullptr);
        }

        mDescriptorSetLayouts.clear();
        mPushConstants.clear();
        mVkShaderModules.clear();
    }

    void VulkanShader::CreateVulkanDescriptorSetLayouts()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        // Iterate through all the sets and creating the layouts
        // (descriptor layouts use HashMap<Uint, VkDescriptorSetLayout> because the Uint specifies at which set number
        // the layout is going to be used
        const Vector<Uint>& descriptorSetCount = mReflectionData.GetDescriptorSetCount();
        for (const Uint& descriptorSet : descriptorSetCount)
        {
            Vector<VkDescriptorSetLayoutBinding> layoutBindings;
            for (const ShaderBuffer& buffer : mReflectionData.GetBuffers())
            {
                if (buffer.Set != descriptorSet)
                    continue;

                VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
                layoutBinding.binding = buffer.Binding;
                layoutBinding.descriptorCount = 1; // TODO: Need to add arrays
                layoutBinding.descriptorType = VulkanUtils::ShaderBufferUsageToVulkan(buffer.ShaderUsage);
                layoutBinding.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(buffer.ShaderStages);
            }

            for (const ShaderResource& texture : mReflectionData.GetResources())
            {
                if (texture.Set != descriptorSet)
                    continue;

                VkDescriptorSetLayoutBinding& LayoutBinding = layoutBindings.emplace_back();
                LayoutBinding.binding = texture.Binding;
                LayoutBinding.descriptorCount = 1; // TODO: Need to add arrays
                LayoutBinding.descriptorType = VulkanUtils::ShaderImageUsageToVulkan(texture.ShaderUsage);
                LayoutBinding.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(texture.ShaderStages);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.flags = 0;
            layoutInfo.bindingCount = static_cast<Uint>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.data();

            VK_CALL(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &mDescriptorSetLayouts[descriptorSet]));
        }
    }

    void VulkanShader::CreateVulkanPushConstantRanges()
    {
        for (const ShaderPushConstant& pushConstant : mReflectionData.GetPushConstantBuffers())
        {
            VkPushConstantRange& pushConstantRange = mPushConstants[pushConstant.BufferName];
            pushConstantRange.offset = 0;
            pushConstantRange.size = pushConstant.Size;
            pushConstantRange.stageFlags = VulkanUtils::GetShaderStagesFlagsFromShaderTypes(pushConstant.ShaderStages);
        }
    }

    void VulkanShader::ParseShader()
    {
        String source = Filesystem::ReadFile(mPath);
        HashCode currentHash = Hash().Generate<String>(source);

        // The source is same, return
        if (mUnparsedShaderHashCode == currentHash)
            return;

        const char* typeToken = "[SurgeShader:";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = source.find(typeToken, 0);
        while (pos != std::string::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            size_t begin = pos + typeTokenLength + 1;
            String type = source.substr(begin, eol - begin);
            size_t nextLinePos = source.find_first_not_of("\r\n", eol);

            ShaderType shaderType = VulkanUtils::ShaderTypeFromString(type);
            SG_ASSERT((int)shaderType, "Invalid shader type!");
            pos = source.find(typeToken, nextLinePos);
            mShaderSources[shaderType] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
            mHashCodes[shaderType] = Hash().Generate<String>(mShaderSources.at(shaderType));
        }

        mUnparsedShaderHashCode = currentHash;
    }
} // namespace Surge