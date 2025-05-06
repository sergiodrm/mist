#version 460

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec2 a_UVs;

layout (location = 0) out vec2 outUV;

layout (set = 0, binding = 0) uniform CameraBlock
{
	mat4 view;
	mat4 projection;
	mat4 viewProjection;
	mat4 invView;
	mat4 invViewProjection;
} u_camera;

layout (set = 1, binding = 0) uniform TransformBlock
{
	mat4 transform;
} u_model;

void main() 
{
	gl_Position = u_camera.invViewProjection * u_model.transform * vec4(a_Pos, 1.0f);  
	outUV = a_UVs;
}