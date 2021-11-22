// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"
#include "SurgeMath/Math.hpp"

namespace Surge
{
    Scene::Scene(bool runtime)
    {
    }

    Scene::~Scene()
    {
        mRegistry.clear();
    }

    void Scene::OnRuntimeStart()
    {
        // TODO: Instantiate scripts, create physics scene here
    }

    void Scene::OnRuntimeEnd()
    {
        // Cleanup scripts and physics system here
    }

    void Scene::Update(const EditorCamera& camera)
    {
        Renderer* renderer = Core::GetRenderer();
        renderer->BeginFrame(camera);
        {
            auto group = mRegistry.group<MeshComponent>(entt::get<TransformComponent>);
            for (auto& entity : group)
            {
                glm::mat4 transform = GetWorldSpaceTransformMatrix(Entity {entity, this});
                auto [mesh, transformComponent] = group.get<MeshComponent, TransformComponent>(entity);
                if (mesh.Mesh)
                {
                    glm::mat4 transform = GetWorldSpaceTransformMatrix(Entity {entity, this});
                    renderer->SubmitMesh(mesh, transform);
                }
            }
        }
        {
            auto view = mRegistry.view<PointLightComponent>();
            for (auto& entity : view)
            {
                auto light = view.get<PointLightComponent>(entity);
                glm::mat4 transformMatrix = GetWorldSpaceTransformMatrix(Entity {entity, this});
                glm::vec3 position, rotation, scale;
                Math::DecomposeTransform(transformMatrix, position, rotation, scale);
                renderer->SubmitPointLight(light, position);
            }
        }
        renderer->EndFrame();
    }

    void Scene::Update()
    {
        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();

        if (camera.Data1)
        {
            Renderer* renderer = Core::GetRenderer();
            renderer->BeginFrame(*camera.Data1, camera.Data2);
            {
                auto group = mRegistry.group<TransformComponent>(entt::get<MeshComponent>);
                for (auto& entity : group)
                {
                    auto [transform, mesh] = group.get<TransformComponent, MeshComponent>(entity);
                    if (mesh.Mesh)
                        renderer->SubmitMesh(mesh, transform.GetTransform());
                }
            }
            {
                auto view = mRegistry.view<PointLightComponent>();
                for (auto& entity : view)
                {
                    auto light = view.get<PointLightComponent>(entity);
                    glm::mat4 transformMatrix = GetWorldSpaceTransformMatrix(Entity {entity, this});
                    glm::vec3 position, rotation, scale;
                    Math::DecomposeTransform(transformMatrix, position, rotation, scale);
                    renderer->SubmitPointLight(light, position);
                }
            }
            renderer->EndFrame();
        }
    }

    template <typename T>
    static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const HashMap<UUID, entt::entity>& enttMap)
    {
        auto components = srcRegistry.view<T>();
        for (entt::entity srcEntity : components)
        {
            entt::entity destEntity = enttMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);

            auto& srcComponent = srcRegistry.get<T>(srcEntity);
            auto& destComponent = dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
        }
    }

    void Scene::CopyTo(Scene* other)
    {
        HashMap<UUID, entt::entity> enttMap;
        auto idComponents = mRegistry.view<IDComponent>();
        for (entt::entity entity : idComponents)
        {
            UUID uuid = mRegistry.get<IDComponent>(entity).ID;
            Entity e;
            other->CreateEntityWithID(e, uuid, "");
            enttMap[uuid] = e.Raw();
        }

        // TODO: Copy ParentChildComponent to runtime
        CopyComponent<NameComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<TransformComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<MeshComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CameraComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<PointLightComponent>(other->mRegistry, mRegistry, enttMap);
    }

    Surge::Entity Scene::FindEntityByUUID(UUID id)
    {
        auto view = mRegistry.view<IDComponent>();
        for (const auto& entity : view)
        {
            auto& idComponent = mRegistry.get<IDComponent>(entity);
            if (idComponent.ID == id)
                return Entity(entity, this);
        }

        return Entity {};
    }

    void Scene::CreateEntity(Entity& outEntity, const String& name)
    {
        entt::entity e = mRegistry.create();
        outEntity = Entity(e, this);
        outEntity.AddComponent<IDComponent>();
        outEntity.AddComponent<NameComponent>(name);
        outEntity.AddComponent<TransformComponent>();
        outEntity.AddComponent<ParentChildComponent>();
    }

    void Scene::CreateEntityWithID(Entity& outEntity, const UUID& id, const String& name)
    {
        entt::entity e = mRegistry.create();
        outEntity = Entity(e, this);
        outEntity.AddComponent<IDComponent>(id);
        outEntity.AddComponent<NameComponent>(name);
        outEntity.AddComponent<TransformComponent>();
        outEntity.AddComponent<ParentChildComponent>();
    }

    void Scene::ParentEntity(Entity& entity, Entity& parent)
    {
        ParentChildComponent& parentChildComponent = entity.GetComponent<ParentChildComponent>();

        // Case where the entity to be parented is the child of something else
        if (parent.IsChildOf(entity))
        {
            // Unparent the parent first
            UnparentEntity(parent);

            Entity newParent = FindEntityByUUID(entity.GetParentUUID());
            if (newParent)
            {
                UnparentEntity(entity);
                ParentEntity(parent, newParent);
            }
        }
        else
        {
            // Unparent if 'entity' was parented to something else before
            Entity previousParent = FindEntityByUUID(entity.GetParentUUID());
            if (previousParent)
                UnparentEntity(entity);
        }

        parentChildComponent.ParentID = parent.GetUUID();
        parent.GetComponent<ParentChildComponent>().ChildIDs.push_back(entity.GetUUID());
        ConvertToLocalSpace(entity);
    }

    glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity)
    {
        glm::mat4 transform(1.0f);

        Entity parent = FindEntityByUUID(entity.GetParentUUID());
        if (parent)
            transform = GetWorldSpaceTransformMatrix(parent);

        return transform * entity.GetComponent<TransformComponent>().GetTransform();
    }

    void Scene::ConvertToLocalSpace(Entity entity)
    {
        Entity parent = FindEntityByUUID(entity.GetParentUUID());

        if (!parent)
            return;

        TransformComponent& transform = entity.GetComponent<TransformComponent>();
        glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);

        glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
        Math::DecomposeTransform(localTransform, transform.Position, transform.Rotation, transform.Scale);
    }

    void Scene::UnparentEntity(Entity& child)
    {
        // Check if the entity has a valid parent
        Entity parent = FindEntityByUUID(child.GetParentUUID());
        if (!parent)
            return;

        // Get the child UUIDs
        auto& children = parent.GetComponent<ParentChildComponent>().ChildIDs;

        // Remove the child from the children UUID list
        children.erase(std::remove(children.begin(), children.end(), child.GetUUID()), children.end());

        ConvertToWorldSpace(child);

        // Set the Parent to be NULL, because, hey we just unparented the entity
        child.GetComponent<ParentChildComponent>().ParentID = 0;
    }

    void Scene::ConvertToWorldSpace(Entity entity)
    {
        // Get the parent
        Entity parent = FindEntityByUUID(entity.GetParentUUID());
        if (!parent)
            return;

        // Get the transformation matrix
        glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);

        TransformComponent& entityTransform = entity.GetComponent<TransformComponent>();
        Math::DecomposeTransform(transform, entityTransform.Position, entityTransform.Rotation, entityTransform.Scale);
    }

    void Scene::DestroyEntity(Entity entity)
    {
        ParentChildComponent& pcc = entity.GetComponent<ParentChildComponent>();

        // Destroy all the child entities, if any
        for (UUID childID : pcc.ChildIDs)
        {
            Entity child = FindEntityByUUID(childID);
            if (child)
            {
                String childName = child.GetComponent<NameComponent>().Name;
                Log<Severity::Debug>("Destroyed Child->({0})", childName);
                DestroyEntity(child); // Destroy the childs recursively
            }
        }

        mRegistry.destroy(entity.Raw());
    }

    void Scene::OnResize(float width, float height)
    {
        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();
        if (camera.Data1)
            camera.Data1->SetViewportSize(width, height);
    }

    Pair<RuntimeCamera*, glm::mat4> Scene::GetMainCameraEntity()
    {
        Pair<RuntimeCamera*, glm::mat4> result = {nullptr, glm::mat4(1.0f)};
        auto view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto& entity : view)
        {
            const auto& [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
            if (camera.Primary)
            {
                result = {&camera.Camera, transform.GetTransform()};
                break;
            }
        }
        return result;
    }
} // namespace Surge