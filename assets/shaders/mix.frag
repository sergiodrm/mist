#version 460

layout(location = 0) in vec2 InTexCoords;
layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D u_tex0;
layout (set = 0, binding = 1) uniform sampler2D u_tex1;
layout (set = 0, binding = 2) uniform UBO
{
    float Alpha;
};

void main()
{
    vec4 col1 = texture(u_tex0, InTexCoords);
    vec4 col2 = texture(u_tex1, InTexCoords);
    outColor = mix(col1, col2, Alpha);
}