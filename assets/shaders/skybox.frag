#version 460

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D u_FrontTex;
layout (set = 0, binding = 1) uniform sampler2D u_BackTex;
layout (set = 0, binding = 2) uniform sampler2D u_LeftTex;
layout (set = 0, binding = 3) uniform sampler2D u_RightTex;
layout (set = 0, binding = 4) uniform sampler2D u_TopTex;
layout (set = 0, binding = 5) uniform sampler2D u_BottomTex;

void main()
{
    outFragColor = texture(u_FrontTex, inTexCoords);
}