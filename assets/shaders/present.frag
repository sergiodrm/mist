#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inTexCoords;

layout(set = 0, binding = 0) uniform sampler2D u_Texture;

void main()
{
    outColor = vec4(inTexCoords, 0.f, 1.f);
    outColor = texture(u_Texture, inTexCoords);
}