#version 460

layout (location = 0) in vec2 inUVs;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D tex;

void main() 
{
	outColor = texture(tex, inUVs);
	//if (inUVs.x > 0.2 || inUVs.y > 0.2)
	//	outColor = vec4(inUVs.xy, 0, 1);
}