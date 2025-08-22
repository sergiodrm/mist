#version 460

#include <shaders/includes/vertex_mesh.glsl>
#include <shaders/includes/camera.glsl>

layout (location = 0) out vec3 outUV;
layout (location = 1) out mat4 outView;

layout (set = 0, binding = 0) uniform CameraBlock
{
	Camera data;
} u_camera;

void main() 
{
	outUV = inPosition;
	gl_Position = u_camera.data.viewProjection * vec4(inPosition.xyz, 1.0f);
	outView = u_camera.data.view;
}