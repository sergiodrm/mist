#version 460

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform UBO
{
  int DebugIndex;
} u_ubo;
layout (set = 1, binding = 0) uniform sampler2D u_tex;

void main() 
{
  outFragColor = texture(u_tex, inUV);
}