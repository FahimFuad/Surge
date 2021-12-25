// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/Titlebar.hpp"
#include "Panels/PaneManager.hpp"
#include "Surge/Project/Project.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "ProjectBrowser.hpp"

namespace Surge
{
    class Editor : public Surge::Client
    {
    public:
        Editor() = default;
        virtual ~Editor() = default;

        virtual void OnInitialize() override;
        virtual void OnUpdate() override;
        virtual void OnImGuiRender() override;
        virtual void OnEvent(Event& e) override;
        virtual void OnShutdown() override;

        // Editor specific
        void OnRuntimeStart();
        void OnRuntimeEnd();

        Project& GetActiveProject() { return mActiveProject; }

        PanelManager& GetPanelManager() { return mPanelManager; }
        Titlebar& GetTitlebar() { return mTitleBar; }
        EditorCamera& GetCamera() { return mCamera; }

    private:
        void Resize();

    private:
        EditorCamera mCamera;
        Renderer* mRenderer;

        PanelManager mPanelManager;
        Titlebar mTitleBar {};
        Project mActiveProject;
        ProjectBrowserWindow mProjectBrowser;
        friend class ProjectBrowserWindow;
    };
} // namespace Surge