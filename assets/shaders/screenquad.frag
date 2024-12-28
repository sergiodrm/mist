#version 460

layout (location = 0) in vec2 inUV;
layout (location = 1) in flat int inTexIndex;

layout (location = 0) out vec4 outFragColor;

layout (set = 1, binding = 0) uniform sampler2D u_tex[8];

float LinearizeDepth(float z, float n, float f)
{
  return (2.0 * n) / (f + n - z * (f - n));
}

void main() 
{
  outFragColor = texture(u_tex[inTexIndex], inUV);
  //float d = LinearizeDepth(texture(u_tex[inTexIndex], inUV).r, 1.f, 1000.f);
  //outFragColor = vec4(d, d, d, 1.f);
}