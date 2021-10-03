// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Shader/Shader.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanGraphicsPipeline.hpp"
#include "Surge/Graphics/Shader/ReflectionData.hpp"
#include "Surge/Graphics/Image.hpp"
#include <shaderc/shaderc.h>

namespace Surge::VulkanUtils
{
    ShaderType ShaderTypeFromString(const String& type);
    String ShaderTypeToString(const ShaderType& type);
    shaderc_shader_kind ShadercShaderKindFromSurgeShaderType(const ShaderType& type);

    VkPrimitiveTopology GetVulkanPrimitiveTopology(PrimitiveTopology primitive);
    VkFormat ShaderDataTypeToVulkanFormat(ShaderDataType type);
    Vector<VkDescriptorSetLayout> GetDescriptorSetLayoutVectorFromHashMap(const HashMap<Uint, VkDescriptorSetLayout>& descriptorSetLayouts);
    Vector<VkPushConstantRange> GetPushConstantRangesVectorFromHashMap(const HashMap<String, VkPushConstantRange>& pushConstants);
    VkDescriptorType ShaderBufferTypeToVulkan(ShaderBuffer::Usage type);
    VkDescriptorType ShaderImageTypeToVulkan(ShaderResource::Usage type);
    VkShaderStageFlags GetShaderStagesFlagsFromShaderTypes(const Vector<ShaderType>& shaderStages);
    void CreateWindowSurface(VkInstance instance, Window* windowHandle, VkSurfaceKHR* surface);
    VkShaderStageFlagBits GetVulkanShaderStage(ShaderType type);
    VkCompareOp GetVulkanCompareOp(CompareOperation op);
    VkPolygonMode GetVulkanPolygonMode(PolygonMode mode);
    VkCullModeFlags GetVulkanCullModeFlags(CullMode mode);
    bool IsDepthFormat(ImageFormat imageFormat);
    Uint GetMemorySize(ImageFormat format, Uint width, Uint height);

    // Image Related
    VkFormat GetImageFormat(ImageFormat format);
    VkFilter GetImageFiltering(TextureFilter filtering);
    VkSamplerAddressMode GetImageAddressMode(TextureAddressMode wrap);
    VkImageUsageFlags GetImageUsageFlags(ImageUsage usage, ImageFormat format);

    void InsertImageMemoryBarrier(
        VkCommandBuffer cmdbuffer, VkImage& image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        VkImageSubresourceRange subresourceRange);
}
