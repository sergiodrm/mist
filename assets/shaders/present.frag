#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inTexCoords;

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputDepth;

void main()
{
    outColor = vec4(inTexCoords, 0.f, 1.f);
}