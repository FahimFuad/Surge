// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Window.hpp"
#include "Pch.hpp"

#ifdef SURGE_WINDOWS
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#endif

namespace Surge
{
    Scope<Window> Window::Create(const WindowData& windowData)
    {
#ifdef SURGE_WINDOWS
        return CreateScope<WindowsWindow>(windowData);
#else
        SG_ASSERT_INTERNAL("Only windows currently is supported!");
#endif
    }
} // namespace Surge
