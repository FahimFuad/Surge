// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Project/Project.hpp"
#include "Surge/Core/Events/Event.hpp"
#include "Surge/Core/Window/Window.hpp"

namespace Surge
{
    struct ClientOptions
    {
        WindowDesc WindowDescription;
        bool EnableImGui = true;
    };

    class SURGE_API Client
    {
    public:
        Client() = default;
        virtual ~Client() = default;

        virtual void OnInitialize() {};
        virtual void OnUpdate() {};
        virtual void OnEvent(Event& e) {};
        virtual void OnImGuiRender() {};
        virtual void OnShutdown() {};
        Project& GetActiveProject() { return mActiveProject; }

        void SetOptions(const ClientOptions& appCreateInfo) { mClientOptions = appCreateInfo; }
        const ClientOptions& GeClientOptions() const { return mClientOptions; }

    protected:
        Project mActiveProject;

    private:
        ClientOptions mClientOptions;
    };

    template <typename T>
    FORCEINLINE T* MakeClient()
    {
        static_assert(std::is_base_of_v<Client, T>, "Class MUST derive from Surge::Client");
        T* client = new T();
        return client;
    }

} // namespace Surge