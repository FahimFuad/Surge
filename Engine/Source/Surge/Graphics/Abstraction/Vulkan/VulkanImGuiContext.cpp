// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanImGuiContext.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderer.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanImage.hpp"
#include <ImGui/Backends/imgui_impl_vulkan.h>
#include <ImGui/Backends/imgui_impl_win32.h>

namespace Surge
{
    static void ImGuiCheckVkResult(VkResult err) { VK_CALL(err); }

    void VulkanImGuiContext::Initialize(void* vulkanRenderContext)
    {
        mVulkanRenderContext = vulkanRenderContext;
        VulkanRenderContext* renderContext = static_cast<VulkanRenderContext*>(vulkanRenderContext);
        VulkanDevice* vulkanDevice = &renderContext->mDevice;
        VkDevice logicalDevice = vulkanDevice->GetLogicalDevice();

        VkDescriptorPoolSize poolSizes[] =
            {{VK_DESCRIPTOR_TYPE_SAMPLER, 100},
             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
             {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
             {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100}};

        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100 * IM_ARRAYSIZE(poolSizes);
        poolInfo.poolSizeCount = (Uint)IM_ARRAYSIZE(poolSizes);
        poolInfo.pPoolSizes = poolSizes;
        VK_CALL(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &mImguiPool));

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
        io.FontDefault = io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/Ruda-Medium.ttf", 14.0f);
        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(SurgeCore::GetWindow()->GetNativeWindowHandle());
        ImGui_ImplVulkan_InitInfo initInfo {};
        initInfo.Instance = renderContext->mVulkanInstance;
        initInfo.PhysicalDevice = vulkanDevice->GetPhysicalDevice();
        initInfo.Device = logicalDevice;
        initInfo.QueueFamily = vulkanDevice->GetQueueFamilyIndices().GraphicsQueue;
        initInfo.Queue = vulkanDevice->GetGraphicsQueue();
        initInfo.DescriptorPool = mImguiPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = 3;
        initInfo.Allocator = VK_NULL_HANDLE;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.CheckVkResultFn = ImGuiCheckVkResult;
        ImGui_ImplVulkan_Init(&initInfo, renderContext->mSwapChain.GetVulkanRenderPass());

        VkCommandBuffer cmd = nullptr;
        vulkanDevice->InstantSubmit(VulkanQueueType::Graphics, [&](VkCommandBuffer& cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

        ImGui_ImplVulkan_DestroyFontUploadObjects();
        SetDarkThemeColors();
    }

    void VulkanImGuiContext::Destroy()
    {
        VulkanRenderContext* renderContext = static_cast<VulkanRenderContext*>(mVulkanRenderContext);
        vkDestroyDescriptorPool(renderContext->mDevice.GetLogicalDevice(), mImguiPool, nullptr);
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplVulkan_Shutdown();
        ImGui::DestroyContext();
    }

    void VulkanImGuiContext::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void VulkanImGuiContext::Render()
    {
        VulkanRenderContext* vkContext;
        SURGE_GET_VULKAN_CONTEXT(vkContext);
        VulkanSwapChain* swapchain = vkContext->GetSwapChain();
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), swapchain->GetVulkanCommandBuffers()[vkContext->GetFrameIndex()]);
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void VulkanImGuiContext::EndFrame()
    {
        // Empty, because why not?
    }

    void* VulkanImGuiContext::AddImage(const Ref<Image2D>& image2d) const
    {
        VulkanRenderer* renderer = static_cast<VulkanRenderer*>(SurgeCore::GetRenderer());
        VkDescriptorSetLayout descriptorSetLayout = ImGui_ImplVulkan_GetDescriptorSetLayout();
        VkDescriptorSet descriptorSet;

        // Allocate the descriptor set (for the texture)
        VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        descriptorSet = renderer->AllocateDescriptorSet(allocInfo);

        // Add the texture
        Ref<VulkanImage2D> vulkanImage2d = image2d.As<VulkanImage2D>();
        ImGui_ImplVulkan_AddTexture(vulkanImage2d->GetVulkanImageView(), vulkanImage2d->GetVulkanImageLayout(), vulkanImage2d->GetVulkanSampler(), descriptorSet);
        return descriptorSet;
    }

    void VulkanImGuiContext::SetDarkThemeColors()
    {
        constexpr auto colorFromBytes = [](const uint8_t r, const uint8_t g, const uint8_t b) {
            return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, 1.0f);
        };

        auto& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        style.TabRounding = 3.5f;
        style.FrameRounding = 3.5f;
        //style.FrameBorderSize = 1.0f;
        style.PopupRounding = 3.5f;
        style.ScrollbarRounding = 3.5f;
        style.GrabRounding = 3.5f;
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.DisplaySafeAreaPadding = ImVec2(0, 0);

        // Headers
        colors[ImGuiCol_Header] = colorFromBytes(62, 62, 62);
        colors[ImGuiCol_HeaderHovered] = colorFromBytes(56, 56, 56);

        // Checbox
        colors[ImGuiCol_CheckMark] = colorFromBytes(255, 255, 255);

        // Buttons
        colors[ImGuiCol_Button] = colorFromBytes(25, 25, 25);
        colors[ImGuiCol_ButtonHovered] = colorFromBytes(110, 110, 110);
        colors[ImGuiCol_ButtonActive] = colorFromBytes(120, 120, 120);

        // Frame
        colors[ImGuiCol_FrameBg] = colorFromBytes(25, 25, 25);
        colors[ImGuiCol_FrameBgHovered] = colorFromBytes(88, 88, 88);
        colors[ImGuiCol_FrameBgActive] = colorFromBytes(110, 110, 110);

        // Tabs
        colors[ImGuiCol_Tab] = colorFromBytes(56, 56, 56);
        colors[ImGuiCol_TabHovered] = colorFromBytes(56, 56, 56);
        colors[ImGuiCol_TabActive] = colorFromBytes(90, 90, 90);
        colors[ImGuiCol_TabUnfocused] = colorFromBytes(40, 40, 40);
        colors[ImGuiCol_TabUnfocusedActive] = colorFromBytes(88, 88, 88);

        // Title
        colors[ImGuiCol_TitleBg] = colorFromBytes(40, 40, 40);
        colors[ImGuiCol_TitleBgActive] = colorFromBytes(40, 40, 40);

        // Others
        colors[ImGuiCol_WindowBg] = colorFromBytes(45, 45, 45);
        colors[ImGuiCol_PopupBg] = colorFromBytes(45, 45, 45);
        colors[ImGuiCol_DockingPreview] = colorFromBytes(26, 26, 26);
        colors[ImGuiCol_TitleBg] = {0.12, 0.12, 0.12, 1.0};
        colors[ImGuiCol_TitleBgActive] = {0.14, 0.14, 0.14, 1.0};
        //colors[ImGuiCol_Separator] = colorFromBytes(10, 200, 10);
        //colors[ImGuiCol_Border] = colorFromBytes(10, 200, 10);
    }

} // namespace Surge
