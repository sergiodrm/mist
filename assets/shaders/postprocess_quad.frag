#version 460

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

// layout (set = 0, binding = 0) uniform sampler2D rt_lighting;
// layout (set = 0, binding = 1) uniform sampler2D rt_lighting_depthbuffer;
// layout (set = 0, binding = 2) uniform sampler2D rt_ui;
// layout (set = 0, binding = 3) uniform sampler2D rt_ui_depthbuffer;
// layout (set = 0, binding = 4) uniform sampler2D rt_gbuffer;
// layout (set = 0, binding = 5) uniform sampler2D rt_composition;

layout (set = 0, binding = 0) uniform sampler2D rts[6];

layout (set = 0, binding = 1) uniform UBO
{
  int DebugIndex;
} u_ubo;

void main() 
{
  if (u_ubo.DebugIndex < 0)
  {
    outFragColor = texture(rts[0], inUV);
  }
  else
  {
     outFragColor = texture(rts[u_ubo.DebugIndex], inUV);
  }
}