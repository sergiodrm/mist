#version 460

layout (set = 0, binding = 0) uniform sampler2D u_tex;

#ifdef BLOOM_DOWNSAMPLE
layout (set = 1, binding = 0) uniform UBO
{
    vec2 Resolution;
} u_BloomDownsampleParams;
#else
#ifdef BLOOM_UPSAMPLE
layout (set = 1, binding = 0) uniform UBO
{
    float FilterRadius;
} u_BloomUpsampleParams;
#else
#ifdef BLOOM_FILTER
layout (set = 1, binding = 0) uniform UBO
{
    vec2 resolution;
    vec2 padding;
    vec3 filterCurve;
    float threshold;
} u_filterParams;
#else
#error
#endif
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

vec4 Upscale(vec2 TexCoords, float FilterRadius)
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = FilterRadius;
    float y = FilterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(u_tex, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
    vec3 b = texture(u_tex, vec2(TexCoords.x,     TexCoords.y + y)).rgb;
    vec3 c = texture(u_tex, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;

    vec3 d = texture(u_tex, vec2(TexCoords.x - x, TexCoords.y)).rgb;
    vec3 e = texture(u_tex, vec2(TexCoords.x,     TexCoords.y)).rgb;
    vec3 f = texture(u_tex, vec2(TexCoords.x + x, TexCoords.y)).rgb;

    vec3 g = texture(u_tex, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
    vec3 h = texture(u_tex, vec2(TexCoords.x,     TexCoords.y - y)).rgb;
    vec3 i = texture(u_tex, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    vec3 upsample = e*4.0;
    upsample += (b+d+f+h)*2.0;
    upsample += (a+c+g+i);
    upsample *= 1.0 / 16.0;
    return vec4(upsample, 1.f);
}

#ifdef BLOOM_FILTER
vec4 QuadraticThreshold(vec4 color, float threshold, vec3 curve)
{
    // max pixel brightness
    float brightness = max(max(color.r, color.g), color.b);
    float rq = clamp(brightness - curve.x, 0.f, curve.y);
    rq = (rq * rq) * curve.z;
    const float epsilon = 0.0001;
    color *= max(rq, brightness - threshold) / max(brightness, epsilon);
    return color;
}

vec4 FilterImage(vec2 texCoords, vec2 resolution, float threshold, vec3 curve)
{
    //vec2 texCoords = vec2(pixel) / textureSize(inputTex, 0);
    //vec4 color = texture(inputTex, texCoords);

    const float clampValue = 20.f;
    vec4 color = Downscale(texCoords, resolution);
    //return color;
    color = clamp(color, vec4(0.f), vec4(clampValue));
    color = QuadraticThreshold(color, threshold, curve);

    if (!(color.r > threshold || color.g > threshold || color.b > threshold))
        color = vec4(0.f);
    return color;
}
#endif

void main()
{
#ifdef BLOOM_DOWNSAMPLE
    vec2 texres = u_BloomDownsampleParams.Resolution;
    FragColor = Downscale(InTexCoords, texres);
#else
#ifdef BLOOM_UPSAMPLE
    float filterRadius = u_BloomUpsampleParams.FilterRadius;
    FragColor = Upscale(InTexCoords, filterRadius);
#else
#ifdef BLOOM_FILTER
    FragColor = FilterImage(InTexCoords, u_filterParams.resolution, u_filterParams.threshold, u_filterParams.filterCurve);
#else
#error
#endif
#endif
#endif
}
