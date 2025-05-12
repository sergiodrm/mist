#version 460

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) in vec3 inNormal;

layout (location = 0) out vec4 outColor;

layout (set = 2, binding = 0) uniform MaterialBlock
{
	vec4 albedo;
} u_material;

void main() 
{
	outColor = u_material.albedo;
	outColor = vec4(inTexCoords.xy, 0.0, 1.f);
}