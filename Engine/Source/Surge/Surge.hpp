// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

// NOTE(Rid): Never include this file in Engine codebase, only used by client applications
#include "Core/Core.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Core/Logger/Logger.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/Thread/ThreadPool.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Window/Window.hpp"

#include "Surge/Events/Event.hpp"

#include "Surge/Graphics/GraphicsPipeline.hpp"
#include "Surge/Graphics/IndexBuffer.hpp"
#include "Surge/Graphics/RenderContext.hpp"
#include "Surge/Graphics/Shader/Shader.hpp"
#include "Surge/Graphics/Texture.hpp"
#include "Surge/Graphics/VertexBuffer.hpp"

#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"

#include <imgui.h>
#include "Surge/Utility/Filesystem.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/type_ptr.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
