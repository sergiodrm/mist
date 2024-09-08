#version 460

layout (set = 0, binding = 0) uniform sampler2D u_gradientTex;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec4 InColor;

void main()
{
#if 0
    vec2 Coord = gl_PointCoord - vec2(0.5);
    FragColor = vec4(InColor.rgb, 0.5 - length(Coord));
    const float radius = 0.5f;
    if (length(Coord) > radius)
        discard;
#else
    float alpha = texture(u_gradientTex, gl_PointCoord).r;
    FragColor = vec4(InColor.rgb, 1.f - InColor.a * alpha);
    if (alpha <= 0.1f)
        discard;
#endif
}
