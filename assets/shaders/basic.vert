#version 460

layout (location = 0) in vec3 LSPosition;

layout (location = 0) out vec3 outColor;

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
    vec4 pos = camera.ViewProjection * objects.Data[gl_BaseInstance].Transform * vec4(LSPosition, 1.0f);
    gl_Position = pos;
    outColor = LSPosition;
}