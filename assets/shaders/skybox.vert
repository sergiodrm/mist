#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inUV;

layout (location = 0) out vec3 outUV;
layout (location = 1) out mat4 outView;

layout (set = 0, binding = 0) uniform UBO
{
	mat4 View;
	mat4 ProjModel;
} u_ubo;

void main() 
{
	outUV = inPos;
	gl_Position = u_ubo.ProjModel * vec4(inPos.xyz, 1.0f);
	outView = u_ubo.View;
}