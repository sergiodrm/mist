#version 460

layout (location = 0) in vec3 LSPosition;
layout (location = 1) in vec3 LSNormal;
layout (location = 2) in vec3 VIColor;
layout (location = 3) in vec2 TexCoords;

layout (location = 0) out vec4 outFragPos;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec2 outTexCoords;

layout (std140, set = 0, binding = 0) uniform CameraBuffer
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} camera;


layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer
{
    mat4 TransformArray[];
} objects;

void main()
{
    vec4 wsPos = objects.TransformArray[gl_BaseInstance] * vec4(LSPosition, 1.0f);
    gl_Position = camera.ViewProjection * wsPos;
    outFragPos = wsPos;
    outColor = VIColor;
    outNormal = LSNormal;
    outTexCoords = TexCoords;
}