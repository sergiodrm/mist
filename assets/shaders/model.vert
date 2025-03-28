#version 460

#include <shaders/includes/vertex_mesh.glsl>

//layout (location = 0) in vec3 LSPosition;
//layout (location = 1) in vec3 LSNormal;
//layout (location = 2) in vec3 VIColor;
//layout (location = 3) in vec3 Tangent;
//layout (location = 4) in vec2 TexCoords;

layout (location = 0) out vec4 outFragPos;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec2 outTexCoords;

// Per frame data
layout (std140, set = 0, binding = 0) uniform CameraBuffer
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} u_camera;

// Per draw data
layout (std140, set = 1, binding = 0) uniform Object
{
    mat4 ModelMatrix;
} u_model;

void main()
{
    vec4 wsPos = u_model.ModelMatrix * vec4(inPosition, 1.0f);
    gl_Position = u_camera.ViewProjection * wsPos;
    outFragPos = wsPos;
    outColor = inColor;
    outNormal = inNormal;
    outTexCoords = inUV0;
}