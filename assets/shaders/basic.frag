#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoords;

layout(set = 1, binding = 0) uniform sampler2D tex1;

void main()
{
    float amplitude = 10.f;
    outColor = vec4((inColor.y + amplitude*0.5f) / amplitude, 0.4f, (1.f - (inColor.y + amplitude * 0.5f) / amplitude), 1.f);
    outColor = texture(tex1, inTexCoords);
    if (inTexCoords.y > 0.2f)
        outColor.xyz = vec3(inTexCoords, 0.2f);
}