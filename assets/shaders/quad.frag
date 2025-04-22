#version 460

layout (location = 0) in vec2 inUVs;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D tex;

void main() 
{
	outColor = texture(tex, inUVs);
}