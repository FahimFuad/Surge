// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"

namespace Surge
{
    ShaderType VulkanUtils::ShaderTypeFromString(const String& type)
    {
        if (type == "Vertex]")
            return ShaderType::Vertex;
        if (type == "Pixel]")
            return ShaderType::Pixel;
        if (type == "Compute]")
            return ShaderType::Compute;

        return ShaderType::None;
    }

    String VulkanUtils::ShaderTypeToString(const ShaderType& type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return "Vertex";
            case ShaderType::Pixel: return "Pixel";
            case ShaderType::Compute: return "Compute";
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }
        return "";
    }

    shaderc_shader_kind VulkanUtils::ShadercShaderKindFromSurgeShaderType(const ShaderType& type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return shaderc_glsl_vertex_shader;
            case ShaderType::Pixel: return shaderc_glsl_fragment_shader;
            case ShaderType::Compute: return shaderc_glsl_compute_shader;
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }

        return static_cast<shaderc_shader_kind>(-1);
    }

    VkPrimitiveTopology VulkanUtils::GetVulkanPrimitiveTopology(PrimitiveTopology primitive)
    {
        switch (primitive)
        {
            case PrimitiveTopology::Points: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case PrimitiveTopology::Lines: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case PrimitiveTopology::Triangles: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case PrimitiveTopology::None: SG_ASSERT_INTERNAL("PrimitiveType::None is invalid!");
        }

        SG_ASSERT_INTERNAL("No Surge::PrimitiveType maps to VkPrimitiveTopology!");
        return VkPrimitiveTopology();
    }

    VkFormat VulkanUtils::ShaderDataTypeToVulkanFormat(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float: return VK_FORMAT_R32_SFLOAT;
            case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
            case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
            case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
            default: SG_ASSERT_INTERNAL("Undefined!");
        }

        SG_ASSERT_INTERNAL("No Surge::ShaderDataType maps to VkFormat!");
        return VK_FORMAT_UNDEFINED;
    }

    Vector<VkDescriptorSetLayout> VulkanUtils::GetDescriptorSetLayoutVectorFromHashMap(const HashMap<Uint, VkDescriptorSetLayout>& descriptorSetLayouts)
    {
        Vector<VkDescriptorSetLayout> descriptorSetLayout;
        for (auto& layout : descriptorSetLayouts)
            descriptorSetLayout.push_back(layout.second);
        return descriptorSetLayout;
    }

    Vector<VkPushConstantRange> VulkanUtils::GetPushConstantRangesVectorFromHashMap(const HashMap<String, VkPushConstantRange>& pushConstants)
    {
        Vector<VkPushConstantRange> pushConstantsVector;
        for (auto& pushConstant : pushConstants)
            pushConstantsVector.push_back(pushConstant.second);
        return pushConstantsVector;
    }

    VkDescriptorType VulkanUtils::ShaderBufferTypeToVulkan(ShaderBuffer::Usage type)
    {
        switch (type)
        {
            case ShaderBuffer::Usage::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case ShaderBuffer::Usage::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        SG_ASSERT(false, "ShaderBuffer::Usage is invalid");
        return VkDescriptorType();
    }

    VkDescriptorType VulkanUtils::ShaderImageTypeToVulkan(ShaderResource::Usage type)
    {
        switch (type)
        {
            case ShaderResource::Usage::Sampled: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case ShaderResource::Usage::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        SG_ASSERT(false, "ShaderResource::Usage is invalid");
        return VkDescriptorType();
    }

    VkShaderStageFlags VulkanUtils::GetShaderStagesFlagsFromShaderTypes(const Vector<ShaderType>& shaderStages)
    {
        VkShaderStageFlags stageFlags {};
        for (const ShaderType& stage : shaderStages)
        {
            // None = 0, VertexShader, PixelShader, ComputeShader
            switch (stage)
            {
                case ShaderType::Vertex: stageFlags |= VK_SHADER_STAGE_VERTEX_BIT; break;
                case ShaderType::Pixel: stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT; break;
                case ShaderType::Compute: stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT; break;
                case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid!");
            }
        }
        return stageFlags;
    }

    void VulkanUtils::CreateWindowSurface(VkInstance instance, Window* windowHandle, VkSurfaceKHR* surface)
    {
#ifdef SURGE_WINDOWS
        PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;

        // Getting the vkCreateWin32SurfaceKHR function pointer and assert if it doesnt exist
        vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
        if (!vkCreateWin32SurfaceKHR)
            SG_ASSERT_INTERNAL("[Win32] Vulkan instance missing VK_KHR_win32_surface extension");

        VkWin32SurfaceCreateInfoKHR sci;
        memset(&sci, 0, sizeof(sci)); // Clear the info
        sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sci.hinstance = GetModuleHandle(nullptr);
        sci.hwnd = static_cast<HWND>(windowHandle->GetNativeWindowHandle());

        VK_CALL(vkCreateWin32SurfaceKHR(instance, &sci, nullptr, surface));
#else
        SG_ASSERT_INTERNAL("Surge is currently Windows Only! :(");
#endif
    }

    VkFormat VulkanUtils::GetImageFormat(ImageFormat format)
    {
        switch (format)
        {
            case ImageFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ImageFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case ImageFormat::Depth32: return VK_FORMAT_D32_SFLOAT;
            case ImageFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
            case ImageFormat::None: SG_ASSERT_INTERNAL("ImageFormat::None is invalid!");
        }
        SG_ASSERT_INTERNAL("Invalid ImageFormat!");
        return VK_FORMAT_UNDEFINED;
    }

    VkFilter VulkanUtils::GetImageFiltering(TextureFilter filtering)
    {
        switch (filtering)
        {
            case TextureFilter::Linear: return VK_FILTER_LINEAR;
            case TextureFilter::Nearest: return VK_FILTER_NEAREST;
        }
        SG_ASSERT_INTERNAL("Invalid TextureFilter!");
        return VK_FILTER_MAX_ENUM;
    }

    VkSamplerAddressMode VulkanUtils::GetImageAddressMode(TextureAddressMode wrap)
    {
        switch (wrap)
        {
            case TextureAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case TextureAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case TextureAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case TextureAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        }
        SG_ASSERT_INTERNAL("Invalid TextureAddressMode!");
        return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }

    VkImageUsageFlags VulkanUtils::GetImageUsageFlags(ImageUsage usage, ImageFormat format)
    {
        VkImageUsageFlags vkImageUsage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: Maybe not force this?
        switch (usage)
        {
            case ImageUsage::Attachment:
            {
                if (VulkanUtils::IsDepthFormat(format))
                    vkImageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                else
                    vkImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                break;
            }
            case ImageUsage::Texture: vkImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; break;
            case ImageUsage::Storage: vkImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; break;
            case ImageUsage::None: SG_ASSERT_INTERNAL("ImageUsage::None is invalid!"); break;
        }

        return vkImageUsage;
    }

    void VulkanUtils::InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer, VkImage& image,
                                               VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                               VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                                               VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                               VkImageSubresourceRange subresourceRange)
    {
        VkImageMemoryBarrier imageMemoryBarrier {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        imageMemoryBarrier.srcAccessMask = srcAccessMask;
        imageMemoryBarrier.dstAccessMask = dstAccessMask;
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    VkShaderStageFlagBits VulkanUtils::GetVulkanShaderStage(ShaderType type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderType::Pixel: return VK_SHADER_STAGE_FRAGMENT_BIT;
            case ShaderType::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }
        SG_ASSERT_INTERNAL("Invalid shader type specified!");
        return VkShaderStageFlagBits();
    }

    VkPolygonMode VulkanUtils::GetVulkanPolygonMode(PolygonMode mode)
    {
        switch (mode)
        {
            case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
            case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
            case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
            case PolygonMode::None: SG_ASSERT_INTERNAL("PolygonMode::None is invalid in this case!");
        }
        SG_ASSERT_INTERNAL("Invalid polygon mode specified!");
        return VK_POLYGON_MODE_MAX_ENUM;
    }

    VkCullModeFlags VulkanUtils::GetVulkanCullModeFlags(CullMode mode)
    {
        switch (mode)
        {
            case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
            case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
            case CullMode::None: return VK_CULL_MODE_NONE;
        }
        SG_ASSERT_INTERNAL("Invalid cull mode specified!");
        return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
    }

    VkCompareOp VulkanUtils::GetVulkanCompareOp(CompareOperation op)
    {
        switch (op)
        {
            case CompareOperation::Never: return VK_COMPARE_OP_NEVER;
            case CompareOperation::Less: return VK_COMPARE_OP_LESS;
            case CompareOperation::Equal: return VK_COMPARE_OP_EQUAL;
            case CompareOperation::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareOperation::Greater: return VK_COMPARE_OP_GREATER;
            case CompareOperation::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
            case CompareOperation::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case CompareOperation::Always: return VK_COMPARE_OP_ALWAYS;
        }
        SG_ASSERT_INTERNAL("Invalid DepthCompareOperation specified!");
        return VK_COMPARE_OP_MAX_ENUM;
    }

    bool VulkanUtils::IsDepthFormat(ImageFormat imageFormat)
    {
        switch (imageFormat)
        {
            case ImageFormat::RGBA8:
            case ImageFormat::RGBA16F:
            case ImageFormat::RGBA32F: return false;
            case ImageFormat::Depth32:
            case ImageFormat::Depth24Stencil8: return true;
            case ImageFormat::None: SG_ASSERT_INTERNAL("ImageFormat::None is invalid in this case!"); break;
        }
        SG_ASSERT_INTERNAL("ImageFormat is invalid!");
        return false;
    }

    Uint VulkanUtils::GetMemorySize(ImageFormat format, Uint width, Uint height)
    {
        switch (format)
        {
            case ImageFormat::RGBA8: return width * height * (4);                   // 32 bit(4 byte) per pixel
            case ImageFormat::RGBA16F: return width * height * (4 * 2);             // 64 bit(8 byte) per pixel
            case ImageFormat::RGBA32F: return width * height * (4 * sizeof(float)); // 128 bit(16 byte) per pixel
        }
        SG_ASSERT_INTERNAL("Invalid ImageFormat!");
        return 0;
    }
} // namespace Surge
