#version 460


layout (location = 0) in vec3 LSPosition;
layout (location = 1) in vec3 LSNormal;
layout (location = 2) in vec3 VIColor;
layout (location = 3) in vec3 Tangent;
layout (location = 4) in vec2 TexCoords;

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
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} u_Camera;

//layout (std140, set = 0, binding = 1) uniform DepthInfo
//{
//    mat4 LightMatrix[3];
//} u_depthInfo;

// Per draw data
layout (std140, set = 1, binding = 0) uniform Object
{
    mat4 ModelMatrix;
} u_model;

void main()
{
    vec4 wsPos = u_model.ModelMatrix * vec4(LSPosition, 1.0f);
    gl_Position = u_Camera.ViewProjection * wsPos;

    // Frag position in view space
    outFragPos = u_Camera.View * wsPos;
    // Normal dir in view space
    mat3 normalTransform = mat3(u_Camera.View * u_model.ModelMatrix);
    outNormal = normalize(normalTransform * normalize(LSNormal));
    vec3 tangent = normalize(normalTransform * normalize(Tangent));
    // Calculate TBN matrix with View Space
    vec3 B = cross(outNormal, tangent);
    outTBN = mat3(tangent, B, outNormal);

    outColor = VIColor;
    outTexCoords = TexCoords;
#if 0
    outLightSpaceFragPos_0 = u_depthInfo.LightMatrix[0] * outFragPos;
    outLightSpaceFragPos_1 = u_depthInfo.LightMatrix[1] * outFragPos;
    outLightSpaceFragPos_2 = u_depthInfo.LightMatrix[2] * outFragPos;
#else
    outLightSpaceFragPos_0 = outFragPos;
    outLightSpaceFragPos_1 = outFragPos;
    outLightSpaceFragPos_2 = outFragPos;
#endif
}