#version 460

#define _CAMERA_NOT_PREV_FRAME_
#include <shaders/includes/camera.glsl>
#include <shaders/includes/vertex_mesh.glsl>

layout (location = 0) out vec3 outUV;
layout (location = 1) out mat4 outView;


void main() 
{
	outUV = inPosition;
	gl_Position = u_camera.data.viewProjection * vec4(inPosition.xyz, 1.0f);
	outView = u_camera.data.view;
}