#version 460

#include <shaders/includes/camera.glsl>

layout (location = 0) out vec3 outUV;
layout (location = 1) out mat4 outView;

layout (set = 0, binding = 0) uniform CameraBlock
{
	Camera data;
} u_camera;

#define _VIEW_PROJ_MATRIX u_camera.data.viewProjection
#include <shaders/includes/vertex_mesh.glsl>

void main() 
{
	outUV = inPosition;
	gl_Position = Vertex_ComputeToClipSpace(vec4(inPosition.xyz, 1.0f));
	outView = u_camera.data.view;
}