#version 460

layout (location = 0) in vec2 a_Pos;
layout (location = 1) in vec2 a_UVs;
layout (location = 2) in int a_TexIndex;

layout (location = 0) out vec2 outUV;
layout (location = 1) out int outTexIndex;

layout (set = 0, binding = 0) uniform CameraData
{
	mat4 OrthoProjection;
} u_camera;

void main() 
{
	outTexIndex = a_TexIndex;
	outUV = a_UVs;
	gl_Position = u_camera.OrthoProjection * vec4(a_Pos, 0.f, 1.0f);
}