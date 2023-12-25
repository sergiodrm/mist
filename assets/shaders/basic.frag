#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoords;

layout(set = 1, binding = 0) uniform sampler2D texArray[3];

void main()
{
    float amplitude = 10.f;
    outColor = vec4((inColor.y + amplitude*0.5f) / amplitude, 0.4f, (1.f - (inColor.y + amplitude * 0.5f) / amplitude), 1.f);
    outColor = texture(texArray[2], inTexCoords);
    // if (outColor.a <= 0.1f)
    //     discard;
    outColor = vec4(inColor, 1.f);
}