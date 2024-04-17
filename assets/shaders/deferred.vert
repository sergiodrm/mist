#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUVs;

layout (location = 0) out vec2 outUV;

void main() 
{
	gl_Position = vec4(inPos, 1.f);
	outUV = inUVs;
}
