#version 460



layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform UBO 
{
	float zNear;
	float zFar;
} ubo;

layout (set = 1, binding = 0) uniform sampler2D samplerColor;

float LinearizeDepth(float depth)
{
  float n = ubo.zNear;
  float f = ubo.zFar;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main() 
{
	//float depth = texture(samplerColor, inUV).r;
	//outFragColor = vec4(vec3(1.0-LinearizeDepth(depth)), 1.0);
  outFragColor = texture(samplerColor, inUV);
}