#version 460

layout (location = 0) in vec3 LSPosition;

layout (set = 0, binding = 0) uniform CameraBuffer
{
    mat4 View;
    mat4 Projection;
    mat4 ViewProjection;
} camera;

void main()
{
    vec4 pos = camera.ViewProjection * vec4(LSPosition, 1.0f);
    gl_Position = pos;
}