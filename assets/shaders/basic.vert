#version 460

layout (location = 0) in vec3 LSPosition;
layout (location = 1) in vec3 VIColor;
layout (location = 2) in vec2 TexCoords;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outTexCoords;

layout (set = 0, binding = 0) uniform CameraBuffer
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} camera;

struct ObjectData
{
    mat4 Transform;
};

layout (std140, set = 0, binding = 1) readonly buffer ObjectBuffer
{
    ObjectData Data[];
} objects;

void main()
{
    vec4 wsPos = objects.Data[gl_BaseInstance].Transform * vec4(LSPosition, 1.0f);
    gl_Position = camera.ViewProjection * wsPos;
    outColor = VIColor;
    outTexCoords = TexCoords;
}