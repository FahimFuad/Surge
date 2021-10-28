// Copyright (c) - SurgeTechnologies - All rights reserved
[SurgeShader: Vertex]
#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTangent;
layout(location = 3) in vec3 aBiTangent;
layout(location = 4) in vec2 aTexCoord;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vTexCoord;

layout(push_constant) uniform PushConstants
{
    mat4 ViewProjection;
    mat4 Transform;
} uFrameData;

void main()
{
    vTexCoord = aTexCoord;
    vNormal = normalize(aNormal);
    gl_Position = uFrameData.ViewProjection * uFrameData.Transform * vec4(aPosition, 1.0);
}

[SurgeShader: Pixel]
#version 460 core

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vTexCoord;

layout(binding = 0, set = 0) uniform Material
{
   vec3 Albedo;

} uMaterial;


void main()
{
    outColor = vec4(uMaterial.Albedo, 1.0);
}