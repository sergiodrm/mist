#version 460

layout (set = 0, binding = 0) uniform sampler2D u_tex;
layout (set = 0, binding = 1) uniform UBO 
{
    vec4 u_Threshold;
};

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 InTexCoords;

void main()
{
    vec4 color = texture(u_tex, InTexCoords);

    if (color.r >= u_Threshold.r || color.g >= u_Threshold.g || color.b >= u_Threshold.b)
        FragColor = color;
    else
        FragColor = vec4(0.f, 0.f, 0.f, color.a);
}
