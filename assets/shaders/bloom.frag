#version 460

layout (set = 0, binding = 0) uniform sampler2D u_tex;
layout (set = 1, binding = 0) uniform UBO
{
    vec2 Resolution;
} u_ubo;

#ifndef BLOOM_DOWNSAMPLE
#ifndef BLOOM_UPSAMPLE
#error Shader must be configured to downsample or upsample
#endif
#endif

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 InTexCoords;

vec4 Downscale(vec2 TexCoords, vec2 TexResolution)
{
    vec2 texelSize = 1.f / TexResolution;
    float x = texelSize.x;
    float y = texelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(u_tex, vec2(TexCoords.x - 2*x, TexCoords.y + 2*y)).rgb;
    vec3 b = texture(u_tex, vec2(TexCoords.x,       TexCoords.y + 2*y)).rgb;
    vec3 c = texture(u_tex, vec2(TexCoords.x + 2*x, TexCoords.y + 2*y)).rgb;

    vec3 d = texture(u_tex, vec2(TexCoords.x - 2*x, TexCoords.y)).rgb;
    vec3 e = texture(u_tex, vec2(TexCoords.x,       TexCoords.y)).rgb;
    vec3 f = texture(u_tex, vec2(TexCoords.x + 2*x, TexCoords.y)).rgb;

    vec3 g = texture(u_tex, vec2(TexCoords.x - 2*x, TexCoords.y - 2*y)).rgb;
    vec3 h = texture(u_tex, vec2(TexCoords.x,       TexCoords.y - 2*y)).rgb;
    vec3 i = texture(u_tex, vec2(TexCoords.x + 2*x, TexCoords.y - 2*y)).rgb;

    vec3 j = texture(u_tex, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
    vec3 k = texture(u_tex, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
    vec3 l = texture(u_tex, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
    vec3 m = texture(u_tex, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    vec4 color = vec4(e*0.125, 1.f);
    color += vec4((a+c+g+i)*0.03125, 1.f);
    color += vec4((b+d+f+h)*0.0625, 1.f);
    color += vec4((j+k+l+m)*0.125, 1.f);
    //color = texture(u_tex, TexCoords);
    //color = vec4(texelSize, 0.f, 1.f);
    return color;
}

vec4 Upscale(vec2 TexCoords, vec2 TexResolution)
{
    vec4 color = vec4(1.f);
    return color;
}

void main()
{
    vec2 texres = u_ubo.Resolution;
#ifdef BLOOM_DOWNSAMPLE
    FragColor = Downscale(InTexCoords, texres);
#else
#ifdef BLOOM_UPSAMPLE
    FragColor = Upscale(InTexCoords, texres);
#else
#error
#endif
#endif
}
