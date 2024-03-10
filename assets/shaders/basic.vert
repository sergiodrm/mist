#version 460

layout (location = 0) in vec3 LSPosition;
layout (location = 1) in vec3 LSNormal;
layout (location = 2) in vec3 VIColor;
layout (location = 3) in vec2 TexCoords;

layout (location = 0) out vec4 outFragPos;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec2 outTexCoords;
layout (location = 4) out vec4 outLightSpaceFragPos_0;
layout (location = 5) out vec4 outLightSpaceFragPos_1;
layout (location = 6) out vec4 outLightSpaceFragPos_2;

// Per frame data
layout (std140, set = 0, binding = 0) uniform CameraBuffer
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} u_Camera;

layout (std140, set = 0, binding = 1) uniform DepthInfo
{
    mat4 LightMatrix[3];
} u_depthInfo;

// Per draw data
layout (std140, set = 1, binding = 0) uniform Object
{
    mat4 ModelMatrix;
} u_Object;

void main()
{
    vec4 wsPos = u_Object.ModelMatrix * vec4(LSPosition, 1.0f);
    gl_Position = u_Camera.ViewProjection * wsPos;
    outFragPos = wsPos;
    outColor = VIColor;
    outNormal = LSNormal;
    outTexCoords = TexCoords;
    outLightSpaceFragPos_0 = u_depthInfo.LightMatrix[0] * u_Object.ModelMatrix * vec4(LSPosition, 1.f);
    outLightSpaceFragPos_1 = u_depthInfo.LightMatrix[1] * u_Object.ModelMatrix * vec4(LSPosition, 1.f);
    outLightSpaceFragPos_2 = u_depthInfo.LightMatrix[2] * u_Object.ModelMatrix * vec4(LSPosition, 1.f);
}