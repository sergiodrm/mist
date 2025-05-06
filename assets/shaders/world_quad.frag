#version 460

layout (location = 0) in vec2 inUVs;

layout (location = 0) out vec4 outColor;

layout (set = 2, binding = 0) uniform MaterialBlock
{
	vec4 color;
} u_material;

void main() 
{
	outColor = u_material.color;
}