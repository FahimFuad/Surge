// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <Surge/Surge.hpp>
#include "Panels/Titlebar.hpp"
#include "Panels/PaneManager.hpp"

namespace Surge
{
    enum class SceneState
    {
        Edit,
        Play
    };

    class Editor : public Application
    {
    public:
        virtual void OnInitialize() override;
        virtual void OnUpdate() override;
        virtual void OnImGuiRender() override;
        virtual void OnEvent(Event& e) override;
        virtual void OnShutdown() override;

        // Editor specific
        void OnRuntimeStart();
        void OnRuntimeEnd();

        SceneState GetSceneState() { return mSceneState; }
        PanelManager& GetPanelManager() { return mPanelManager; }
        Titlebar& GetTitlebar() { return mTitleBar; }

    private:
        void Resize();

    private:
        EditorCamera mCamera;
        Renderer* mRenderer;

        Ref<Scene> mEditorScene, mRuntimeScene;

        Entity mEntity;
        Entity mOtherEntity;

        PanelManager mPanelManager;
        Titlebar mTitleBar {};
        SceneState mSceneState;
    };

} // namespace Surge
