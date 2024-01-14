#version 460

layout (location = 0) in vec3 a_Position;

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
    gl_Position = camera.ViewProjection * vec4(a_Position, 1.f);
}