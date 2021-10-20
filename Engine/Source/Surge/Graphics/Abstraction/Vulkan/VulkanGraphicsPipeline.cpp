// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanGraphicsPipeline.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderCommandBuffer.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanShader.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanFramebuffer.hpp"

namespace Surge
{
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(const GraphicsPipelineSpecification& pipelineSpec)
        : mSpecification(pipelineSpec)
    {
        Reload();
        mSpecification.Shader->AddReloadCallback([&]() { this->Reload(); }); // Recreate the pipeline if shader gets reloaded
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        Clear();
    }

    void VulkanGraphicsPipeline::Reload()
    {
        Clear();

        SCOPED_TIMER("Pipeline ({0})", mSpecification.DebugName);
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();

        // Setting up all the shaders into a create info class
        HashMap<ShaderType, VkShaderModule> shaderModules = mSpecification.Shader.As<VulkanShader>()->GetVulkanShaderModules();
        Vector<VkPipelineShaderStageCreateInfo> shaderStages;
        for (const auto& shader : shaderModules)
        {
            VkPipelineShaderStageCreateInfo& shaderStageInfo = shaderStages.emplace_back();
            shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageInfo.stage = VulkanUtils::GetVulkanShaderStage(shader.first);
            shaderStageInfo.module = shader.second;
            shaderStageInfo.pName = "main";
            shaderStageInfo.pSpecializationInfo = nullptr;
        }

        const ShaderReflectionData& reflectedData = mSpecification.Shader->GetReflectionData();

        // We only need the stage input of vertex shader to generate the input layout
        const std::map<Uint, ShaderStageInput>& stageInputs = reflectedData.GetStageInputs().at(ShaderType::Vertex);

        // Calculate the stride
        Uint stride = 0;
        for (const auto& [location, stageInput] : stageInputs)
            stride += stageInput.Size;

        VkVertexInputBindingDescription vertexBindingDescriptions;
        vertexBindingDescriptions.binding = 0;
        vertexBindingDescriptions.stride = stride;
        vertexBindingDescriptions.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        Vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions(stageInputs.size());
        for (Uint i = 0; i < stageInputs.size(); i++)
        {
            const ShaderStageInput& input = stageInputs.at(i);
            vertexAttributeDescriptions[i].binding = 0;
            vertexAttributeDescriptions[i].location = i;
            vertexAttributeDescriptions[i].format = VulkanUtils::ShaderDataTypeToVulkanFormat(input.DataType);
            vertexAttributeDescriptions[i].offset = input.Offset;
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<Uint>(vertexAttributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescriptions;
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VulkanUtils::GetVulkanPrimitiveTopology(mSpecification.Topology);
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        inputAssembly.flags = 0;

        // Setting the viewport and scissors to nullptr because later we will use dynamic pipeline states that avoids
        // pipeline recreation on window resize
        VkPipelineViewportStateCreateInfo viewportState {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.scissorCount = 1;
        viewportState.pScissors = nullptr;

        // Rasterizer stage
        VkPipelineRasterizationStateCreateInfo rasterizer {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VulkanUtils::GetVulkanPolygonMode(mSpecification.PolygonMode);
        rasterizer.lineWidth = mSpecification.LineWidth;
        rasterizer.cullMode = VulkanUtils::GetVulkanCullModeFlags(mSpecification.CullingMode);
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // TODO: Maybe add as an specification option?
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        // We wont use MSAA, never again
        VkPipelineMultisampleStateCreateInfo multisampling {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth state
        VkPipelineDepthStencilStateCreateInfo depthStencil {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = mSpecification.UseDepth;
        depthStencil.depthWriteEnable = mSpecification.UseDepth;
        depthStencil.depthCompareOp = VulkanUtils::GetVulkanCompareOp(mSpecification.DepthCompOperation);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = mSpecification.UseStencil;

        // Blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // TODO: Enable blending
        VkPipelineColorBlendStateCreateInfo colorBlending {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        // Setting up the pipeline layout
        Ref<VulkanShader> vulkanShader = mSpecification.Shader.As<VulkanShader>();
        Vector<VkDescriptorSetLayout> descriptorSetLayouts = VulkanUtils::GetDescriptorSetLayoutVectorFromHashMap(vulkanShader->GetDescriptorSetLayouts());
        Vector<VkPushConstantRange> pushConstants = VulkanUtils::GetPushConstantRangesVectorFromHashMap(vulkanShader->GetPushConstantRanges());

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = static_cast<Uint>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<Uint>(pushConstants.size());
        pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();
        VK_CALL(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &mPipelineLayout));

        // Using dynamic pipeline states to avoid pipeline recreation when resizing
        std::array<VkDynamicState, 3> dynamicStates;
        dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
        dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
        dynamicStates[2] = VK_DYNAMIC_STATE_LINE_WIDTH;
        VkPipelineDynamicStateCreateInfo dynamicStatesCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicStatesCreateInfo.dynamicStateCount = static_cast<Uint>(dynamicStates.size());
        dynamicStatesCreateInfo.pDynamicStates = dynamicStates.data();

        VkGraphicsPipelineCreateInfo pipelineInfo {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.stageCount = static_cast<Uint>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStatesCreateInfo;
        pipelineInfo.layout = mPipelineLayout;

        if (mSpecification.TargetFramebuffer)
            pipelineInfo.renderPass = mSpecification.TargetFramebuffer.As<VulkanFramebuffer>()->GetVulkanRenderPass();
        else
            pipelineInfo.renderPass = renderContext->GetSwapChain()->GetVulkanRenderPass();

        pipelineInfo.subpass = 0;
        VK_CALL(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline));
    }

    void VulkanGraphicsPipeline::Bind(const Ref<RenderCommandBuffer>& cmdBuffer) const
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);

        VkExtent2D extent;
        if (mSpecification.TargetFramebuffer)
        {
            const FramebufferSpecification& framebufferSpec = mSpecification.TargetFramebuffer->GetSpecification();
            extent.width = framebufferSpec.Width;
            extent.height = framebufferSpec.Height;
        }
        else
            extent = renderContext->GetSwapChain()->GetVulkanExtent2D();

        vkCmdSetLineWidth(vulkanCmdBuffer, mSpecification.LineWidth);
        vkCmdBindPipeline(vulkanCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    }

    void VulkanGraphicsPipeline::SetPushConstantData(const Ref<RenderCommandBuffer>& cmdBuffer, const String& bufferName, void* data) const
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);
        VkPushConstantRange& pushConstant = mSpecification.Shader.As<VulkanShader>()->GetPushConstantRanges()[bufferName];

        SG_ASSERT(pushConstant.stageFlags != 0, "Invalid Push constant name: '{0}'!", bufferName);
        vkCmdPushConstants(vulkanCmdBuffer, mPipelineLayout, pushConstant.stageFlags, pushConstant.offset, pushConstant.size, data);
    }

    void VulkanGraphicsPipeline::DrawIndexed(const Ref<RenderCommandBuffer>& cmdBuffer, Uint indicesCount, Uint baseIndex, Uint baseVertex) const
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        Uint frameIndex = renderContext->GetFrameIndex();
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(frameIndex);

        vkCmdDrawIndexed(vulkanCmdBuffer, indicesCount, 1, baseIndex, baseVertex, 0);
    }

    void VulkanGraphicsPipeline::Clear()
    {
        VulkanRenderContext* renderContext = nullptr;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice device = renderContext->GetDevice()->GetLogicalDevice();
        vkDeviceWaitIdle(device);
        if (mPipeline)
        {
            vkDestroyPipeline(device, mPipeline, nullptr);
            mPipeline = VK_NULL_HANDLE;
        }
        if (mPipelineLayout)
        {
            vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);
            mPipelineLayout = VK_NULL_HANDLE;
        }
    }

} // namespace Surge
