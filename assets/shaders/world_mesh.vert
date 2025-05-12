#version 460

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texCoord0;
layout (location = 3) in vec2 a_texCoord1;
layout (location = 4) in vec4 a_tangent;
layout (location = 5) in vec4 a_vertexColor;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outNormal;

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
	gl_Position = u_camera.invViewProjection * u_model.transform * vec4(a_position, 1.0f);  
	outUV = a_texCoord0;
	outNormal = a_normal;
}