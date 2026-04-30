#version 460

#include <shaders/includes/camera.glsl>

layout (location = 0) out vec4 outFragPos;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec2 outTexCoords;
layout (location = 4) out vec4 outLightSpaceFragPos_0;
layout (location = 5) out vec4 outLightSpaceFragPos_1;
layout (location = 6) out vec4 outLightSpaceFragPos_2;
layout (location = 7) out mat3 outTBN;

// Per frame data
layout (std140, set = 0, binding = 0) uniform CameraBuffer
{
    Camera data;
} u_camera;

layout (std140, set = 0, binding = 1) uniform DepthInfo
{
    mat4 LightMatrix[3];
} u_ShadowMapInfo;

// Per draw data
layout (std140, set = 1, binding = 0) uniform Object
{
    mat4 modelMatrix;
} u_model;

#include <shaders/includes/vertex_mesh.glsl>

void main()
{
    vec4 wsPos = u_model.modelMatrix * vec4(inPosition, 1.0f);
    gl_Position = Vertex_ComputeWorldPosToClipSpace(wsPos);

    // Frag position in view space
    outFragPos = u_camera.data.view * wsPos;
    // Normal dir in view space
    mat3 normalTransform = mat3(u_camera.data.view * u_model.modelMatrix);
    outNormal = normalize(normalTransform * normalize(inNormal));
    vec3 tangent = normalize(normalTransform * normalize(inTangent.xyz));
    // Calculate TBN matrix with View Space
    vec3 B = cross(outNormal, tangent) * inTangent.w;
    outTBN = mat3(tangent, B, outNormal);

    outColor = inColor;
    outTexCoords = inUV0;

    // Precalculate shadow coordinates
    outLightSpaceFragPos_0 = u_ShadowMapInfo.LightMatrix[0] * outFragPos;
    outLightSpaceFragPos_1 = u_ShadowMapInfo.LightMatrix[1] * outFragPos;
    outLightSpaceFragPos_2 = u_ShadowMapInfo.LightMatrix[2] * outFragPos;
}