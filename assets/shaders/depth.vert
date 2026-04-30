#version 460


layout (std140, set = 0, binding = 0) uniform UBO
{
    mat4 DepthVP;
} u_ubo;

layout (std140, set = 1, binding = 0) uniform Model
{
    mat4 Mat;
} u_model;

#define _VIEW_PROJ_MATRIX u_ubo.DepthVP
#include <shaders/includes/vertex_mesh.glsl>

void main()
{
    gl_Position = Vertex_ComputeWorldPosToClipSpace(u_model.Mat * vec4(inPosition, 1.f));
}