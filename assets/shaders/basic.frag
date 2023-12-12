#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 inColor;

void main()
{
    float amplitude = 10.f;
    outColor = vec4((inColor.y + amplitude*0.5f) / amplitude, 0.4f, (1.f - (inColor.y + amplitude * 0.5f) / amplitude), 1.f);
}