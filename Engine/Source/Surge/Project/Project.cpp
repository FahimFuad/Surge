// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Project.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Serializer/Serializer.hpp"

namespace Surge
{
    Project::Project()
    {
        mIsValid = false;
    }

    Project::~Project()
    {
        Destroy();
    }

    void Project::Invalidate(const String& name, const Path& path)
    {
        Destroy();

        mMetadata.Name = name;
        mMetadata.ProjPath = path;
        mMetadata.ProjectID = UUID();
        mMetadata.ActiveSceneIndex = 0;
        mMetadata.InternalDirectory = fmt::format("{0}/{1}", path, ".surge");
        mMetadata.ProjectMetadataPath = fmt::format("{0}/{1}.surgeProj", mMetadata.InternalDirectory, name);

        bool success = Filesystem::CreateOrEnsureDirectory(mMetadata.InternalDirectory);
        SG_ASSERT(success, "Cannot create directory!");
        Filesystem::CreateOrEnsureFile(mMetadata.ProjectMetadataPath);

        // Add a default scene
        Ref<Scene> scene = AddScene("Default", fmt::format("{0}", fmt::format("{0}/Default.surge", mMetadata.ProjPath)));
        Serializer::Deserialize<Scene>("Engine/Assets/Scenes/Default.surge", scene.Raw());
        Serializer::Serialize<Scene>(scene->GetMetadata().ScenePath, scene.Raw());

        // Write the project metadata to file
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
        mIsValid = true;
    }

    void Project::Invalidate(const ProjectMetadata& metadata)
    {
        Destroy();
        mMetadata = metadata;

        bool success = Filesystem::CreateOrEnsureDirectory(mMetadata.InternalDirectory);
        SG_ASSERT(success, "Cannot create directory!");
        Filesystem::CreateOrEnsureFile(mMetadata.ProjectMetadataPath);

        if (metadata.SceneMetadatas.empty())
        {
            // Add a default scene if there is none
            Ref<Scene> scene = AddScene("Default", fmt::format("{0}", fmt::format("{0}/Default.surge", mMetadata.ProjPath)));
            Serializer::Deserialize<Scene>("Engine/Assets/Scenes/Default.surge", scene.Raw()); // Load the defaule scene to the new scene
            Serializer::Serialize<Scene>(scene->GetMetadata().ScenePath, scene.Raw());         // Save the new scene in the project path
        }
        else
        {
            for (const SceneMetadata& sceneMetadata : metadata.SceneMetadatas)
            {
                // We don't use Project::AddScene here, as that adds the scene to the metadata
                // As in this scope we only read from the already filled metadata
                Ref<Scene> scene = Ref<Scene>::Create(sceneMetadata, false);
                mScenes.push_back(scene);
                Serializer::Deserialize<Scene>(sceneMetadata.ScenePath, scene.Raw());
            }
        }

        // Write the project metadata to file
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
        mIsValid = true;
    }

    void Project::OnRuntimeStart()
    {
        mRuntimeSceneStorage.resize(mScenes.size());
        for (Uint i = 0; i < mScenes.size(); i++)
        {
            // Create the runtime scene
            auto& runtimeScene = mRuntimeSceneStorage[i];
            runtimeScene = Ref<Scene>::Create(mScenes[i]->GetMetadata().Name, mScenes[i]->GetMetadata().ScenePath, true);
            mScenes[i]->CopyTo(runtimeScene.Raw()); // Copy the scene
            runtimeScene->OnRuntimeStart();
        }
        Core::GetRenderer()->SetSceneContext(mRuntimeSceneStorage[mMetadata.ActiveSceneIndex]);
    }

    void Project::Update(EditorCamera& camera)
    {
        switch (mProjectState)
        {
            case Surge::ProjectState::Edit:
                mScenes[mMetadata.ActiveSceneIndex]->Update(camera);
                break;
            case Surge::ProjectState::Play:
                mRuntimeSceneStorage[mMetadata.ActiveSceneIndex]->Update();
                break;
        }
    }

    void Project::OnRuntimeEnd()
    {
        for (auto& scene : mRuntimeSceneStorage)
        {
            scene->OnRuntimeEnd();
        }
        mRuntimeSceneStorage.clear();
        Core::GetRenderer()->SetSceneContext(mScenes[mMetadata.ActiveSceneIndex]);
    }

    Ref<Scene> Project::AddScene(const SceneMetadata& metadata)
    {
        Ref<Scene> newScene = Ref<Scene>::Create(metadata, false);
        mScenes.push_back(newScene);
        mMetadata.SceneMetadatas.push_back(newScene->GetMetadata());
        return newScene;
    }

    Ref<Scene> Project::AddScene(const String& name, const Path& path)
    {
        Ref<Scene> newScene = Ref<Scene>::Create(name, path, false);
        mScenes.push_back(newScene);
        mMetadata.SceneMetadatas.push_back(newScene->GetMetadata());
        return newScene;
    }

    void Project::RemoveScene(Uint arrayIndex)
    {
        mScenes.erase(mScenes.begin() + arrayIndex);
    }

    Ref<Scene>& Project::GetScene(Uint arrayIndex)
    {
        return mScenes[arrayIndex];
    }

    void Project::SetActiveScene(Uint sceneindex)
    {
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);

        Ref<Scene>& activatedScene = GetScene(sceneindex);
        for (auto& func : mOnActiveSceneChangeCallbacks)
            func(activatedScene);

        mMetadata.ActiveSceneIndex = sceneindex;
    }

    void Project::Save()
    {
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
        for (auto& scene : mScenes)
            Serializer::Serialize<Scene>(scene->GetMetadata().ScenePath, scene.Raw());
    }

    void Project::Destroy()
    {
        mScenes.clear();
        mRuntimeSceneStorage.clear();
        mOnActiveSceneChangeCallbacks.clear();
        mIsValid = false;
        mMetadata = {};
    }

} // namespace Surge
