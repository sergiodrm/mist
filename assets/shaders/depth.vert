#version 460

#include <shaders/includes/vertex_mesh.glsl>

layout (std140, set = 0, binding = 0) uniform UBO
{
    mat4 DepthVP;
} u_ubo;

layout (std140, set = 1, binding = 0) uniform Model
{
    mat4 Mat;
} u_model;

void main()
{
    gl_Position = u_ubo.DepthVP * u_model.Mat * vec4(inPosition, 1.f);
}