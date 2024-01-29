#version 460

layout (location = 0) in vec3 a_Position;

layout (set = 0, binding = 0) uniform CameraBuffer
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} camera;

void main()
{
    gl_Position = camera.ViewProjection * vec4(a_Position, 1.f);
}