#version 460

layout (location = 0) in vec2 inUV;
layout (location = 1) in flat int inTexIndex;

layout (location = 0) out vec4 outFragColor;

layout (set = 1, binding = 0) uniform sampler2D u_tex[8];

void main() 
{
  outFragColor = texture(u_tex[inTexIndex], inUV);
  //outFragColor = vec4(inUV, 1.f, 1.f);
}