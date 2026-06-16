#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out float fragColor;

layout (set = 0, binding = 0) uniform sampler2D u_ssaoTex;

layout (set = 0, binding = 1) uniform DataBlock
{
    vec2 texelSize;
    vec2 _padding;
} u_data;

void main()
{
    float res = 0.f;
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * u_data.texelSize;
            res += texture(u_ssaoTex, inTexCoords + offset).r;
        }
    }
    res *= 1.f/16.f;
    fragColor = res;
}
