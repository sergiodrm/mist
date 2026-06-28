
#include <shaders/includes/math.glsl>
#include <shaders/includes/pbr.glsl>

/*
 * Data structures for lighting
*/

struct LightData
{
    vec3 Color;
    float Compression;
    

    vec3 Pos;
    float Radius;
    

    vec3 Dir;
    float _padding;
    

    vec2 CosCutoff; // x: inner, y: outer
    int ShadowMapIndex;
    float Strength;
    
};

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 10
#endif

#ifndef MAX_DIRECTIONAL_LIGHTS
#define MAX_DIRECTIONAL_LIGHTS 1
#endif

#ifndef MAX_SPOT_LIGHTS
#define MAX_SPOT_LIGHTS 10
#endif

#ifndef MAX_SHADOW_MAPS
#define MAX_SHADOW_MAPS 3
#endif

struct LightingData
{
    vec3 ambientColor;
    int numOfSpotLights;

    int numOfDirectionalLights;
    int numOfPointLights;
    vec2 _padding;

    LightData pointLights[MAX_POINT_LIGHTS];
    LightData directionalLights[MAX_DIRECTIONAL_LIGHTS];
    LightData spotLights[MAX_SPOT_LIGHTS];
};

/**
 * Input
*/

layout (std140, set = LIGHTING_DATA_SET, binding = 0) uniform LightingDataBlock
{
    LightingData data;
} u_lighting;

layout(set = LIGHTING_DATA_SET, binding = 1) uniform ShadowMapInfo
{
#ifndef LIGHTING_SHADOWS_COORDS
    mat4 LightViewMat[MAX_SHADOW_MAPS];
#endif
    vec4 noiseParams; // w: padding
    vec4 textureOffset3DSize; // xyz: tex3D offset size. w: filter radius
    vec4 textureOffsetParams; // x: window size. y: filter size. zw: shadow map texture size
} u_ShadowMapInfo;

// Irradiance map
layout(set = LIGHTING_TEXTURES_SET, binding = 0) uniform samplerCube u_irradianceMap;
layout(set = LIGHTING_TEXTURES_SET, binding = 1) uniform samplerCube u_prefilterMap;
layout(set = LIGHTING_TEXTURES_SET, binding = 2) uniform sampler2D u_brdfMap;
// Shadow maps
layout(set = LIGHTING_TEXTURES_SET, binding = 3) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];
layout(set = LIGHTING_TEXTURES_SET, binding = 4) uniform sampler2D u_blueNoise;
layout(set = LIGHTING_TEXTURES_SET, binding = 5) uniform sampler3D u_offset3D;

#define LIGHTING_SHADOWS_NOISE_SCALE u_ShadowMapInfo.noiseParams.x
#define LIGHTING_SHADOWS_NOISE_FACTOR u_ShadowMapInfo.noiseParams.y
#define LIGHTING_SHADOWS_NOISE_PCF_KERNEL_SIZE int(u_ShadowMapInfo.noiseParams.z)

#define LIGHTING_SHADOWS_TEXTURE_OFFSET_3D_SIZE u_ShadowMapInfo.textureOffset3DSize.xyz
#define LIGHTING_SHADOWS_TEXTURE_FILTER_RADIUS u_ShadowMapInfo.noiseParams.w
#define LIGHTING_SHADOWS_TEXTURE_WINDOW_SIZE int(u_ShadowMapInfo.textureOffsetParams.x)
#define LIGHTING_SHADOWS_TEXTURE_FILTER_SIZE int(u_ShadowMapInfo.textureOffsetParams.y)

#define LIGHTING_SHADOWS_SHADOW_MAP_SIZE u_ShadowMapInfo.textureOffsetParams.zw



//#define LIGHTING_NO_SHADOWS
//#ifndef LIGHTING_NO_SHADOWS



/**
 * Util functions
*/

float LinearizeDepth(float z, float n, float f)
{
    return (2.0 * n) / (f + n - z * (f - n));	
}

float CalculateAttenuation(float distance, float radius, float compression)
{
#if 1
    return pow(smoothstep(radius, 0, distance), compression);
#elif 1
    return 1.f - smoothstep(radius * 0.75f, radius, distance);
#else
    return 1.f / (distance * distance);
#endif
}


/**
 * Shadows functions
*/

#ifndef LIGHTING_NO_SHADOWS

// Calculate shadow coordinate from ShadowInfo (LightViewMatrix or precalculated fragPos into light space).
#ifndef LIGHTING_SHADOWS_COORDS
vec4 CalculateShadowCoords(int shadowIndex, vec3 fragPos)
{
    vec4 shadowCoord = u_ShadowMapInfo.LightViewMat[shadowIndex] * vec4(fragPos, 1.f);
    shadowCoord = shadowCoord / shadowCoord.w;
    return shadowCoord;
}
#endif // !LIGHTING_SHADOWS_COORDS

float ComputeShadowRandomSampling(int shadowIndex, vec4 shadowCoords, float bias)
{
    ivec3 offsetCoord;
    offsetCoord.yz = ivec2(mod(gl_FragCoord.xy, vec2(LIGHTING_SHADOWS_TEXTURE_WINDOW_SIZE)));
    int samplesDiv2 = int(LIGHTING_SHADOWS_TEXTURE_FILTER_SIZE * LIGHTING_SHADOWS_TEXTURE_FILTER_SIZE / 2.f);
    // Calculate shadow coordinate from ShadowInfo (LightViewMatrix or precalculated fragPos into light space).
    vec4 sc = shadowCoords;

    vec2 texelSize = vec2(1.f,1.f) / LIGHTING_SHADOWS_SHADOW_MAP_SIZE;
    
    float sum = 0.f;
    float depth = 0.f;
    for (int i = 0; i < 4; ++i)
    {
        offsetCoord.x = i;
        vec4 offsets = texelFetch(u_offset3D, offsetCoord, 0) * LIGHTING_SHADOWS_TEXTURE_FILTER_RADIUS * 2.f - 0.5f;
        offsets *= texelSize.xyxy;
        sc.xy = shadowCoords.xy + offsets.xy;
        depth = texture(u_ShadowMap[shadowIndex], sc.xy).x;
        if (depth + bias < shadowCoords.z)
            sum += 0.f;
        else
            sum += 1.f;

        sc.xy = shadowCoords.xy + offsets.zw;
        depth = texture(u_ShadowMap[shadowIndex], sc.xy).x;
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
            vec4 offsets = texelFetch(u_offset3D, offsetCoord, 0) * LIGHTING_SHADOWS_TEXTURE_FILTER_RADIUS* 2.f - 0.5f;
            sc.xy = shadowCoords.xy + offsets.xy * texelSize;
            depth = texture(u_ShadowMap[shadowIndex], sc.xy).x;
            if (depth + bias < shadowCoords.z)
                sum += 0.f;
            else
                sum += 1.f;

            sc.xy = shadowCoords.xy + offsets.zw * texelSize;
            depth = texture(u_ShadowMap[shadowIndex], sc.xy).x;
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
    vec2 texelSize = 1.0 / LIGHTING_SHADOWS_SHADOW_MAP_SIZE;
    vec2 noiseTexelSize = 1.f / vec2(textureSize(u_blueNoise, 0));
    float currentDepth = shadowCoords.z;

    const int kernelSize = LIGHTING_SHADOWS_NOISE_PCF_KERNEL_SIZE;
    const int halfKernelSize = kernelSize/2;

    for (float x = -halfKernelSize; x <= halfKernelSize; ++x)
    {
        for (float y = -halfKernelSize; y <= halfKernelSize; ++y)
        {
            // Get noise for this sample
            vec2 noiseTc = shadowCoords.xy/(noiseTexelSize.xy*LIGHTING_SHADOWS_NOISE_SCALE);
            vec2 noise = texture(u_blueNoise, noiseTc).xy;
            // transform from [0,1] to [-0.5, 0.5]
            noise = noise - 0.5f; 

            // Offset of pixel tc to be filtered
            vec2 offset = vec2(x*texelSize.x, y*texelSize.y);
            offset+=(noise*texelSize*LIGHTING_SHADOWS_NOISE_FACTOR);
            float d = texture(u_ShadowMap[shadowIndex], shadowCoords.xy+offset).r;
            shadow += (d<(shadowCoords.z-bias) ? 0.f : 1.f);
        }
    }
    shadow /= (kernelSize*kernelSize);
	return shadow;
}

float ComputeShadow(int shadowIndex, vec4 shadowCoords, float bias)
{
    float shadow = 0.0;
    float z = shadowCoords.z;
	if ( z > -1.0 && z < 1.0 ) 
	{
		shadow = texture( u_ShadowMap[shadowIndex], shadowCoords.xy ).r;
		if ( shadowCoords.w > 0.0 && z - bias > shadow ) 
			shadow = 0.f;
        else
            shadow = 1.f;
	}
    return shadow;
}

#endif // !LIGHTING_NO_SHADOWS

vec3 ComputeLightShadow(vec3 fragPos, vec3 lightDir, vec3 normal, LightData light, vec3 irradiance
#ifdef LIGHTING_SHADOWS_COORDS
    , vec4 shadowCoords
#endif
    )
{
#ifndef LIGHTING_NO_SHADOWS

    float shadow = 0.f;
    int shadowMapIndex = int(light.ShadowMapIndex);
    if (shadowMapIndex >= 0)
    {
#ifndef LIGHTING_SHADOWS_COORDS
        vec4 shadowCoords = CalculateShadowCoords(light.ShadowMapIndex, fragPos);
#endif // !LIGHTING_SHADOWS_COORDS
        float diffuseFactor = dot(normal, lightDir);
        float bias = mix(0.001, 0.f, diffuseFactor);
#if 0
        shadow = ComputeShadow(shadowMapIndex, shadowCoords, bias);
#elif 0
        shadow = ComputeShadowPCF(shadowMapIndex, shadowCoords, bias);
#else
        shadow = ComputeShadowRandomSampling(light.ShadowMapIndex, shadowCoords, bias);
#endif
    }

#ifdef LIGHTING_SHADOWS_DEBUG
    return shadow.xxx;
#else
    return shadow*irradiance;
#endif

#else
    return irradiance;
#endif // !LIGHTING_NO_SHADOWS
}

/**
 * Lighting functions
*/

// Basic lighting without PBR
vec3 CalculateLighting(vec3 fragPos, vec3 fragNormal, vec3 viewPos, vec3 lightDir, vec3 lightColor)
{
    // Diffuse
    vec3 normal = normalize(fragNormal);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.5f;
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    // TODO: shininess from material
    float shininess = 32.f;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    return diffuse + specular;
}

vec3 ProcessIrradiance(vec3 normal, vec3 view, vec3 albedo, float roughness, float metallic, float ao)
{
    vec3 N = normalize(normal);
    vec3 V = normalize(view);
    //return V;

    vec3 F0 = mix(F0_Default, albedo, metallic);

    float NdotV = max(dot(N, V), 0.0);
    // diffuse irradiance
    vec3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = 1.f-kS;
    kD *= 1.f-metallic;
    vec3 i = texture(u_irradianceMap, N).rgb;
    vec3 diffuse = i * albedo;
    // debug diffuse
    //return kD * diffuse * ao;

    // specular reflection
    vec2 envBrdf = texture(u_brdfMap, vec2(NdotV, roughness)).rg;
    const float MAX_REFLECTION_LOD = 8.f;
    vec3 r = normalize(reflect(-V, N));
    const float mipLevel = roughness * MAX_REFLECTION_LOD;
    vec3 prefilteredColor = textureLod(u_prefilterMap, r, mipLevel).rgb;
    vec3 specular = prefilteredColor * (kS * envBrdf.x + envBrdf.y);
    return (kD * diffuse + specular) * ao;
}

/**
 * Light types calculations
*/

float ComputeSpotLightIntensity(LightData light, vec3 fragPos)
{
    vec3 lightDir = normalize(vec3(light.Pos) - fragPos);
    float theta = dot(lightDir, -vec3(light.Dir));
    float cosCutoff = light.CosCutoff.x;
    float cosOuterCutoff = light.CosCutoff.y;
    float epsilon = max(cosCutoff - cosOuterCutoff, 0.f);
    float intensity = clamp((theta - cosOuterCutoff) / epsilon, 0.0, 1.0);
    return intensity;
}

vec3 ProcessPointLight(vec3 fragPos, vec3 viewDir, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness)
{
    // Radiance calculation with attenuation
    vec3 lightPos = light.Pos.xyz;
    vec3 lightDir = lightPos - fragPos;
    vec3 V = normalize(viewDir);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);
    float distance = length(lightDir);
    float radius = light.Radius;
    float compression = light.Compression;
    float attenuation = CalculateAttenuation(distance, radius, compression);
    vec3 radiance = light.Color.rgb * attenuation * light.Strength;

    return CalculateBRDF(fragNormal, L, radiance, albedo, metallic, roughness, V, H);
}

vec3 ProcessDirectionalLight(vec3 fragPos, vec3 viewDir, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness
#ifdef LIGHTING_SHADOWS_COORDS
    , vec4 shadowCoords
#endif
    )
{
    vec3 V = normalize(viewDir);
    vec3 lightDir = normalize(vec3(-light.Dir));
    vec3 H = normalize(V + lightDir);
    vec3 radiance = light.Color.rgb * light.Strength;
    // Calculate pbr contribution
    vec3 lighting = CalculateBRDF(fragNormal, lightDir, radiance, albedo, metallic, roughness, V, H);
    lighting = ComputeLightShadow(fragPos, lightDir, fragNormal, light, lighting
#ifdef LIGHTING_SHADOWS_COORDS
                        , shadowCoords
#endif
                    );
    return lighting;
}

vec3 ProcessSpotLight(vec3 fragPos, vec3 viewDir, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness
#ifdef LIGHTING_SHADOWS_COORDS
    , vec4 shadowCoords
#endif
    )
{
    vec3 lighting = vec3(0.f);
    float intensity = ComputeSpotLightIntensity(light, fragPos);
    if (intensity > 0.f)
    {
        float radius = light.Radius;
        float compression = light.Compression;
        float distance = length(light.Pos.xyz - fragPos);
        float attenuation = CalculateAttenuation(distance, radius, compression);
        vec3 radiance = light.Color.rgb * attenuation * light.Strength;
        vec3 L = normalize(light.Pos.xyz - fragPos);
        vec3 V = normalize(viewDir);
        vec3 H = normalize(V + L);
        lighting = CalculateBRDF(fragNormal, L, radiance, albedo, metallic, roughness, V, H);
        lighting = ComputeLightShadow(fragPos, L, fragNormal, light, lighting
#ifdef LIGHTING_SHADOWS_COORDS
                        , shadowCoords
#endif
                    );
    }
    return lighting * intensity;
}

//#define LIGHTING_DEBUG_AMBIENT
//#define LIGHTING_DEBUG_LIGHTS 

#define LIGHTING_APPLY_GI

/// DoEnvironmentLighting
/// * fragPos: fragment position in world space
/// * viewDir: view direction -> normalize(fragWorldPos - viewWorldPos)
/// * normal: fragment normal in world space
vec3 DoLighting(vec3 fragPos, vec3 viewDir, vec3 normal, vec3 albedo, float metallic, float roughness, float ao
#ifdef LIGHTING_SHADOWS_COORDS
    , vec4 shadowCoords
#endif
    )
{
    // Point lights
    vec3 pointLightsColor = vec3(0.f);
    for (int i = 0; i < u_lighting.data.numOfPointLights; ++i)
    {
        pointLightsColor += ProcessPointLight(fragPos, viewDir, normal, u_lighting.data.pointLights[i], albedo, metallic, roughness);
    }

    // Spot lights
    vec3 spotLightsColor = vec3(0.f);
    for (int i = 0; i < u_lighting.data.numOfSpotLights; ++i)
    {
        spotLightsColor += ProcessSpotLight(fragPos, viewDir, normal, u_lighting.data.spotLights[i], albedo, metallic, roughness
#ifdef LIGHTING_SHADOWS_COORDS
        , shadowCoords
#endif
        );
    }

    // Directional light
    vec3 directionalLightColor = vec3(0.f);
    for (int i = 0; i < u_lighting.data.numOfDirectionalLights; ++i)
    {
        directionalLightColor += ProcessDirectionalLight(fragPos, viewDir, normal, u_lighting.data.directionalLights[i], albedo, metallic, roughness
#ifdef LIGHTING_SHADOWS_COORDS
        , shadowCoords
#endif
        );
    }

    vec3 lightColor = (pointLightsColor + spotLightsColor + directionalLightColor);

    // Ambient color
#ifdef LIGHTING_APPLY_GI
    vec3 ambientColor = u_lighting.data.ambientColor * ProcessIrradiance(normal, viewDir, albedo, roughness, metallic, ao);
#else
    vec3 ambientColor = u_lighting.data.ambientColor * albedo * ao;
#endif

#if defined(LIGHTING_DEBUG_AMBIENT)
	return ambientColor;
#elif defined(LIGHTING_DEBUG_LIGHTS)
    return lightColor;
#else
    return lightColor + ambientColor;
#endif
}
