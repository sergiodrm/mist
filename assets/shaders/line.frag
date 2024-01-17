#version 460

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sColor {vec4 Color;} u_Color;

void main()
{
    outColor = u_Color.Color;
}