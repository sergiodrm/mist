#version 460

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec4 InColor;

void main()
{
    //FragColor = InColor;
    vec2 Coord = gl_PointCoord - vec2(0.5);
    FragColor = vec4(InColor.rgb, 0.5 - length(Coord));
}
