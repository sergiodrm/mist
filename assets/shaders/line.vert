#version 460

layout (location = 0) in vec4 a_Position;
layout (location = 1) in vec4 a_Color;

layout (location = 0) out vec4 vs_Color;

layout (set = 0, binding = 0) uniform CameraBuffer
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} camera;

void main()
{
    gl_Position = camera.ViewProjection * a_Position;
    vs_Color = a_Color;
}