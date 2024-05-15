#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out float fragColor;

layout (set = 0, binding = 0) uniform sampler2D u_ssaoTex;

void main()
{
    vec2 texelSize = 1.f / vec2(textureSize(u_ssaoTex, 0));
    float res = 0.f;
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            res += texture(u_ssaoTex, inTexCoords + offset).r;
        }
    }
    fragColor = res / (4.f*4.f);
}
