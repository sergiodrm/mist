#version 460

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec2 a_UVs;

layout (location = 0) out vec2 outUV;

void main() 
{
	outUV = a_UVs;
	gl_Position = vec4(a_Pos, 1.0f);
}