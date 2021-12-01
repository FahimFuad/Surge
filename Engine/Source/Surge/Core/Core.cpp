// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Pch.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#include "Surge/Debug/Profiler.hpp"
#include "SurgeReflect/SurgeReflect.hpp"
#include "Thread/ThreadPool.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderContext.hpp"

namespace Surge::Core
{
    static CoreData GCoreData;

    void OnEvent(Event& e)
    {
        GCoreData.SurgeClient->OnEvent(e);
        Surge::EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Surge::WindowResizeEvent>([](Surge::WindowResizeEvent& e) { if (GetWindow()->GetWindowState() != WindowState::Minimized) GCoreData.SurgeRenderContext->OnResize(); });
        dispatcher.Dispatch<Surge::AppClosedEvent>([](Surge::AppClosedEvent& e) { GCoreData.Running = false; });
        dispatcher.Dispatch<Surge::WindowClosedEvent>([](Surge::WindowClosedEvent& e) { GCoreData.Running = false; });
    }

    void Initialize(Client* application)
    {
        SCOPED_TIMER("Core::Initialize");
        SG_ASSERT(application, "Invalid Application!");

        Clock::Start();
        GCoreData.SurgeClient = application;
        const ClientOptions& clientOptions = GCoreData.SurgeClient->GeClientOptions();

        // Window
        GCoreData.SurgeWindow = new WindowsWindow(clientOptions.WindowDescription);
        GCoreData.SurgeWindow->RegisterEventCallback(OnEvent);

        // Render Context
        GCoreData.SurgeRenderContext = new VulkanRenderContext();
        GCoreData.SurgeRenderContext->Initialize(GCoreData.SurgeWindow, clientOptions.EnableImGui);

        // Renderer
        GCoreData.SurgeRenderer = new Renderer();
        GCoreData.SurgeRenderer->Initialize();

        // Reflection Engine
        SurgeReflect::Registry::Initialize();

        GCoreData.Running = true;
        GCoreData.SurgeClient->OnInitialize();
    }

    void Core::Run()
    {
        while (GCoreData.Running)
        {
            SURGE_PROFILE_FRAME("Core::Frame");
            Clock::Update();
            GCoreData.SurgeWindow->Update();

            if (GCoreData.SurgeWindow->GetWindowState() != WindowState::Minimized)
            {
                GCoreData.SurgeRenderContext->BeginFrame();
                GCoreData.SurgeClient->OnUpdate();
                if (GCoreData.SurgeClient->GeClientOptions().EnableImGui)
                    GCoreData.SurgeClient->OnImGuiRender();
                GCoreData.SurgeRenderContext->EndFrame();

                if (!GCoreData.FrameEndCallbacks.empty())
                {
                    for (std::function<void()>& function : GCoreData.FrameEndCallbacks)
                        function();

                    GCoreData.FrameEndCallbacks.clear();
                }
            }
        }
    }

    void Core::Shutdown()
    {
        SCOPED_TIMER("Core::Shutdown");

        // NOTE(Rid): Order Matters here
        GCoreData.SurgeClient->OnShutdown();
        delete GCoreData.SurgeClient;
        GCoreData.SurgeClient = nullptr;

        GCoreData.SurgeRenderer->Shutdown();
        delete GCoreData.SurgeRenderer;
        delete GCoreData.SurgeWindow;
        GCoreData.SurgeRenderContext->Shutdown();
        delete GCoreData.SurgeRenderContext;
        SurgeReflect::Registry::Shutdown();
    }

    void Core::AddFrameEndCallback(const std::function<void()>& func)
    {
        GCoreData.FrameEndCallbacks.push_back(func);
    }

    Window* GetWindow() { return GCoreData.SurgeWindow; }
    RenderContext* GetRenderContext() { return GCoreData.SurgeRenderContext; }
    Renderer* GetRenderer() { return GCoreData.SurgeRenderer; }
    CoreData* GetData() { return &GCoreData; }
    Surge::Client* GetClient() { return GCoreData.SurgeClient; }
} // namespace Surge::Core