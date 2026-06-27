#version 460

layout (set = 1, binding = 0) uniform sampler2D u_gradientTex;

layout(location = 0) out vec4 fragColor;
layout(location = 0) in vec4 inColor;

void main()
{
#if 0
    vec2 Coord = gl_PointCoord - vec2(0.5);
    fragColor = vec4(inColor.rgb, 0.5 - length(Coord));
    const float radius = 0.5f;
    if (length(Coord) > radius)
        discard;
#else
    float alpha = texture(u_gradientTex, gl_PointCoord).r;
    fragColor = vec4(inColor.rgb, alpha);
    fragColor = vec4(1,1,1, alpha);
#endif
}
