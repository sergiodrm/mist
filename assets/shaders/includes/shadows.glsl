

#ifndef _SHADOWS_NOT_DEFINE_UNIFORMS_

#ifndef MAX_SHADOW_MAPS
#define MAX_SHADOW_MAPS 3
#endif

#ifndef _SHADOWS_SET_
#define _SHADOWS_SET_ 4
#endif

layout ( set = _SHADOWS_SET_, binding = 0) uniform ShadowBlock
{
    mat4 transform[MAX_SHADOW_MAPS];

    vec4 textureSize; // xy: shadow maps size, zw: shadow maps texel size
    vec4 params0; // x: filter window size, y: filter size, z: filter radius, w: pcf kernel size
    vec4 params1; // x: pcf noise scale; y: pcf noise factor
} u_shadowData;

layout ( set = _SHADOW_SET_, binding = 1) uniform sampler3D u_offset3D;
layout ( set = _SHADOW_SET_, binding = 2) uniform sampler2D u_blueNoise;
layout ( set = _SHADOW_SET_, binding = 3) uniform sampler2D u_shadowMaps[MAX_SHADOW_MAPS];

#define SHADOWS_MAPS_SIZE                       u_shadowData.textureSize.xy
#define SHADOWS_MAPS_TEXEL_SIZE                 u_shadowData.textureSize.zw

#define SHADOWS_PARAMS_TEXTURE_WINDOW_SIZE      u_shadowData.params0.x
#define SHADOWS_PARAMS_FILTER_SIZE              u_shadowData.params0.y
#define SHADOWS_PARAMS_FILTER_RADIUS            u_shadowData.params0.z

#define SHADOWS_PARAMS_PCF_KERNEL_SIZE          u_shadowData.params0.w
#define SHADOWS_PARAMS_PCF_NOISE_SCALE          u_shadowData.params1.x
#define SHADOWS_PARAMS_PCF_NOISE_FACTOR         u_shadowData.params1.y

#endif

vec4 CalculateShadowCoords(int shadowIndex, vec3 fragPos)
{
    vec4 shadowCoord = u_shadowData.transform[shadowIndex] * vec4(fragPos, 1.f);
    shadowCoord = shadowCoord / shadowCoord.w;
    return shadowCoord;
}

float ComputeShadowRandomSampling(int shadowIndex, vec4 shadowCoords, float bias)
{
    ivec3 offsetCoord;
    offsetCoord.yz = ivec2(mod(gl_FragCoord.xy, vec2(SHADOWS_PARAMS_TEXTURE_WINDOW_SIZE)));
    int samplesDiv2 = int(SHADOWS_PARAMS_FILTER_SIZE * SHADOWS_PARAMS_FILTER_SIZE / 2.f);
    // Calculate shadow coordinate from ShadowInfo (LightViewMatrix or precalculated fragPos into light space).
    vec4 sc = shadowCoords;
    
    float sum = 0.f;
    float depth = 0.f;
    for (int i = 0; i < 4; ++i)
    {
        offsetCoord.x = i;
        vec4 offsets = texelFetch(u_offset3D, offsetCoord, 0) * SHADOWS_PARAMS_FILTER_RADIUS * 2.f - 0.5f;
        offsets *= SHADOWS_MAPS_TEXEL_SIZE.xyxy;
        sc.xy = shadowCoords.xy + offsets.xy;
        depth = texture(u_shadowMaps[shadowIndex], sc.xy).x;
        if (depth + bias < shadowCoords.z)
            sum += 0.f;
        else
            sum += 1.f;

        sc.xy = shadowCoords.xy + offsets.zw;
        depth = texture(u_shadowMaps[shadowIndex], sc.xy).x;
        if (depth + bias < shadowCoords.z)
            sum += 0.f;
        else
            sum += 1.f;
    }

    float shadow = sum / 8.f;
    if (shadow != 0.f && shadow != 1.f)
    {
        for (int i = 4; i < samplesDiv2; ++i)
        {
            offsetCoord.x = i;
            vec4 offsets = texelFetch(u_offset3D, offsetCoord, 0) * SHADOWS_PARAMS_FILTER_RADIUS * 2.f - 0.5f;
            sc.xy = shadowCoords.xy + offsets.xy * SHADOWS_MAPS_TEXEL_SIZE;
            depth = texture(u_shadowMaps[shadowIndex], sc.xy).x;
            if (depth + bias < shadowCoords.z)
                sum += 0.f;
            else
                sum += 1.f;

            sc.xy = shadowCoords.xy + offsets.zw * SHADOWS_MAPS_TEXEL_SIZE;
            depth = texture(u_shadowMaps[shadowIndex], sc.xy).x;
            if (depth + bias < shadowCoords.z)
                sum += 0.f;
            else
                sum += 1.f;
        }

        shadow = sum / float(samplesDiv2 * 2.f);
    }
    return shadow;
}

float ComputeShadowPCF(int shadowIndex, vec4 shadowCoords, float bias)
{
    float shadow = 0.0;

    // TO-DO: pass texture size in uniform buffer
    vec2 noiseTexelSize = 1.f / vec2(textureSize(u_blueNoise, 0));
    float currentDepth = shadowCoords.z;

    const float kernelSize = SHADOWS_PARAMS_PCF_KERNEL_SIZE;
    const float halfKernelSize = kernelSize/2.f;

    for (float x = -halfKernelSize; x <= halfKernelSize; ++x)
    {
        for (float y = -halfKernelSize; y <= halfKernelSize; ++y)
        {
            // Get noise for this sample
            vec2 noiseTc = shadowCoords.xy/(noiseTexelSize.xy * SHADOWS_PARAMS_PCF_NOISE_SCALE);
            vec2 noise = texture(u_blueNoise, noiseTc).xy;
            // transform from [0,1] to [-0.5, 0.5]
            noise = noise - 0.5f; 

            // Offset of pixel tc to be filtered
            vec2 offset = vec2(x * SHADOWS_MAPS_TEXEL_SIZE.x, y * SHADOWS_MAPS_TEXEL_SIZE.y);
            offset += (noise * SHADOWS_MAPS_TEXEL_SIZE * SHADOWS_PARAMS_PCF_NOISE_FACTOR);
            float d = texture(u_shadowMaps[shadowIndex], shadowCoords.xy + offset).r;
            shadow += (d < (shadowCoords.z - bias) ? 0.f : 1.f);
        }
    }
    shadow /= (kernelSize * kernelSize);
	return shadow;
}

float ComputeShadow(int shadowIndex, vec4 shadowCoords, float bias)
{
    float shadow = 0.0;
    float z = shadowCoords.z;
	if ( z > -1.0 && z < 1.0 ) 
	{
		shadow = texture( u_shadowMaps[shadowIndex], shadowCoords.xy ).r;
		if ( shadowCoords.w > 0.0 && z - bias > shadow ) 
			shadow = 0.f;
        else
            shadow = 1.f;
	}
    return shadow;
}

vec3 ComputeLightShadow(vec3 fragPos, vec3 lightDir, vec3 normal, int shadowIndex, vec3 irradiance)
{
    float shadow = 1.f;
    if (shadowIndex >= 0)
    {
        vec4 shadowCoords = CalculateShadowCoords(shadowIndex, fragPos);
        if (shadowCoords.x < 0.f || shadowCoords.x >= 1.f
            ||shadowCoords.y < 0.f || shadowCoords.y >= 1.f)
        {
            shadow = 1.f;
        }
        else
        {
            float diffuseFactor = dot(normal, lightDir);
            float bias = mix(0.01f, 0.001f, diffuseFactor);
            //bias = 0.001f;
#if 0
            shadow = ComputeShadow(shadowIndex, shadowCoords, bias);
#elif 0
            shadow = ComputeShadowPCF(shadowIndex, shadowCoords, bias);
#else
            shadow = ComputeShadowRandomSampling(shadowIndex, shadowCoords, bias);
            //shadow = 0.5f;
#endif
        }
    }
    return shadow*irradiance;
}
