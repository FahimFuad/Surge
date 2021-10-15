// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/ECS/Scene.hpp"

namespace Surge
{
    class SceneHierarchyPanel : public IPanel
    {
    public:
        SceneHierarchyPanel() = default;
        ~SceneHierarchyPanel() = default;

        virtual void Init(void* panelInitArgs);
        virtual void Render(bool* show);
        virtual void Shutdown();
        virtual PanelCode GetCode() const { return GetStaticCode(); }

        static PanelCode GetStaticCode() { return PanelCode::SceneHierarchy; }

        void SetSceneContext(Scene* scene) { mSceneContext = scene; }
        Scene* GetSceneContext(Scene* scene) { return mSceneContext; }

        void SetSelectedEntity(Entity& e) { mSelectedEntity = e; }
        Entity& GetSelectedEntity() { return mSelectedEntity; }

    private:
        void DrawEntityNode(Entity& e);

    private:
        PanelCode mCode;
        Scene* mSceneContext;
        Entity mSelectedEntity; // TODO: Make It a vector when we allow multiple selection

        bool mHierarchyHovered;

        // Renaming stuff
        bool mRenaming;
        String mOldName;
        String mTempBuffer;
    };

} // namespace Surge
