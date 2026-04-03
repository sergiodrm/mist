
#include <shaders/includes/math.glsl>

/*
 * Data structures for lighting
*/

struct LightData
{
    vec3 Color;
    float Compression;
    

    vec3 Pos; // w: pointlight-radius; directional-project shadows (-1.f not project. >=0.f shadow map index)
    float Radius;
    

    vec3 Dir;
    float _padding;
    

    vec2 CosCutoff; // x: inner, y: outer
    int ShadowMapIndex;
    float Strength;
    
};

/*
 * Data structures for lighting
*/
//#define LIGHTING_NO_SHADOWS
#ifndef LIGHTING_NO_SHADOWS

#ifndef LIGHTING_SHADOWS_TEXTURE_ARRAY
#error Macro LIGHTING_SHADOWS_TEXTURE_ARRAY must be define to calculate shadow value
#endif // !LIGHTING_SHADOWS_TEXTURE_ARRAY

#ifndef MAX_SHADOW_MAPS
#error Must define num of shadow maps
#endif // !MAX_SHADOW_MAPS
#endif // !LIGHTING_NO_SHADOWS

#ifndef IRRADIANCE_MAP
#error Must define irradiance map sampler.
#endif

#ifndef PREFILTERED_MAP
#error Must define prefiltered map sampler.
#endif

#ifndef BRDF_MAP
#error Must define brdf map sampler.
#endif

struct ShadowInfo
{
#ifndef LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    vec4 ShadowCoordArray[MAX_SHADOW_MAPS];
#else
    mat4 LightViewMatrices[MAX_SHADOW_MAPS];
#endif // !LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
};


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
 * PBR functions
*/


// The Fresnel-Schlick approximation expects a F0 parameter 
// which is known as the surface reflection at zero incidence 
// or how much the surface reflects if looking directly at the surface.
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.f-F0) * pow(clamp(1.f-cosTheta, 0.f, 1.f), 5.f);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.f);
    float NdotH2 = NdotH*NdotH;

    float num = a2;
    float denom = NdotH2 * (a2 - 1.f) + 1.f;
    denom = M_PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.f;
    float k = (r*r)/8.f;
    float num = NdotV;
    float denom = NdotV * (1.f-k) + k;
    return num/denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.f);
    float NdotL = max(dot(N, L), 0.f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1*ggx2;
}

vec3 GetF0()
{
    return vec3(0.04);
}

vec3 CalculateBRDF(vec3 normal, vec3 lightDir, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 viewDir, vec3 halfway)
{
    vec3 F0 = GetF0();
    F0 = mix(F0, albedo, metallic);
    vec3 F = FresnelSchlick(max(dot(halfway, viewDir), 0.f), F0);

    // Normal distribution
    float NDF = DistributionGGX(normal, halfway, roughness);
    // Geometry distribution
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    // Calculate Cook-Torrance BRDF
    vec3 num = NDF * G * F;
    float denom = 4.f * max(dot(normal, viewDir), 0.f) * max(dot(normal, lightDir), 0.f) + 0.0001f;
    vec3 specular = num/denom;
#ifdef DEBUG_SPECULAR
    return specular;
#endif // DEBUG_SPECULAR

    // Light contribution
    vec3 kS = F; // specular contribution
    vec3 kD = vec3(1.f)-kS; // diffuse contribution (energy conservation)
    kD *= 1.f-metallic;

    float NdotL = max(dot(normal, lightDir), 0.f);
    return (kD * albedo / M_PI + specular) * radiance * NdotL;
}



/**
 * Shadows functions
*/

#ifndef LIGHTING_NO_SHADOWS

#define LIGHTING_SHADOWS_PCF
//#define LIGHTING_SHADOWS_DEBUG

#ifndef LIGHTING_SHADOWS_NOISE_SCALE
#define LIGHTING_SHADOWS_NOISE_SCALE 0.0025
#endif

#ifndef LIGHTING_SHADOWS_NOISE_FACTOR
#define LIGHTING_SHADOWS_NOISE_FACTOR 3
#endif

#ifndef LIGHTING_SHADOWS_NOISE_PCF_KERNEL_SIZE 
#define LIGHTING_SHADOWS_NOISE_PCF_KERNEL_SIZE 9
#endif

float ComputeShadow(ShadowInfo info, vec3 fragPos, int shadowIndex)
{
    // Calculate shadow coordinate from ShadowInfo (LightViewMatrix or precalculated fragPos into light space).
#ifndef LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    vec4 shadowCoord = info.ShadowCoordArray[shadowIndex];
#else
    vec4 shadowCoord = info.LightViewMatrices[shadowIndex] * vec4(fragPos, 1.f);
#endif // !LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    shadowCoord = shadowCoord / shadowCoord.w;

    const float bias = 0.005f;
    float shadow = 0.0;

#if defined(LIGHTING_SHADOWS_PCF)
    // TO-DO: pass texture size in uniform buffer
    vec2 texelSize = 1.0 / vec2(textureSize(LIGHTING_SHADOWS_TEXTURE_ARRAY[shadowIndex], 0));
    vec2 noiseTexelSize = 1.f / vec2(textureSize(u_blueNoise, 0));
    float currentDepth = shadowCoord.z;

    const int kernelSize = LIGHTING_SHADOWS_NOISE_PCF_KERNEL_SIZE;
    const int halfKernelSize = kernelSize/2;

    for (float x = -halfKernelSize; x <= halfKernelSize; ++x)
    {
        for (float y = -halfKernelSize; y <= halfKernelSize; ++y)
        {
            // Get noise for this sample
            vec2 noiseTc = shadowCoord.xy/(noiseTexelSize.xy*LIGHTING_SHADOWS_NOISE_SCALE);
            vec2 noise = texture(u_blueNoise, noiseTc).xy;
            // transform from [0,1] to [-0.5, 0.5]
            noise = noise - 0.5f; 

            // Offset of pixel tc to be filtered
            vec2 offset = vec2(x*texelSize.x, y*texelSize.y);
            offset+=(noise*texelSize*LIGHTING_SHADOWS_NOISE_FACTOR);
            float d = texture(LIGHTING_SHADOWS_TEXTURE_ARRAY[shadowIndex], shadowCoord.xy+offset).r;
            shadow += (d<(shadowCoord.z-bias) ? 0.f : 1.f);
        }
    }
    shadow /= (kernelSize*kernelSize);
#else
    float z = shadowCoord.z;
	if ( z > -1.0 && z < 1.0 ) 
	{
		shadow = texture( u_ShadowMap[shadowIndex], shadowCoord.xy ).r;
		if ( shadowCoord.w > 0.0 && z - bias > shadow ) 
			shadow = 0.f;
        else
            shadow = 1.f;
	}
#endif // defined(LIGHTING_SHADOWS_PCF)
	return shadow;
}

#endif // !LIGHTING_NO_SHADOWS

vec3 ComputeLightShadow(ShadowInfo info, vec3 fragPos, LightData light, vec3 irradiance)
{
#if !defined(LIGHTING_NO_SHADOWS)
    float shadow = 0.f;
    if (light.ShadowMapIndex >= 0)
    {
        shadow = ComputeShadow(info, fragPos, light.ShadowMapIndex);
    }
#if defined(LIGHTING_SHADOWS_DEBUG)
    // Calculate shadow coordinate from ShadowInfo (LightViewMatrix or precalculated fragPos into light space).
#ifndef LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    vec4 shadowCoord = info.ShadowCoordArray[light.ShadowMapIndex];
#else
    vec4 shadowCoord = info.LightViewMatrices[light.ShadowMapIndex] * vec4(fragPos, 1.f);
#endif // !LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    shadowCoord = shadowCoord / shadowCoord.w;
    vec2 noise = texture(u_blueNoise, shadowCoord.xy).xy;
    //return noise.xxy;
    return shadow.xxx;
#endif
    return shadow*irradiance;

#else
    return irradiance;
#endif
}

/**
 * Lighting functions
*/

#define LightRadius(l) l.Pos.a
#define LightCompression(l) l.Color.w

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

    vec3 F0 = GetF0();
    F0 = mix(F0, albedo, metallic);

    float NdotV = max(dot(N, V), 0.0);
    // diffuse irradiance
    vec3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = 1.f-kS;
    kD *= 1.f-metallic;
    vec3 i = texture(IRRADIANCE_MAP, N).rgb;
    vec3 diffuse = i * albedo;
    // debug diffuse
    //return kD * diffuse * ao;

    // specular reflection
    vec2 envBrdf = texture(BRDF_MAP, vec2(NdotV, roughness)).rg;
    const float MAX_REFLECTION_LOD = 8.f;
    vec3 r = normalize(reflect(-V, N));
    const float mipLevel = roughness * MAX_REFLECTION_LOD;
    vec3 prefilteredColor = textureLod(PREFILTERED_MAP, r, mipLevel).rgb;
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

vec3 ProcessPointLight(vec3 fragPos, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness)
{
    // Radiance calculation with attenuation
    vec3 lightPos = light.Pos.xyz;
    vec3 lightDir = lightPos - fragPos;
    vec3 V = normalize(-fragPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);
    float distance = length(lightDir);
    float radius = light.Radius;
    float compression = light.Compression;
    float attenuation = CalculateAttenuation(distance, radius, compression);
    vec3 radiance = light.Color.rgb * attenuation * light.Strength;

    return CalculateBRDF(fragNormal, L, radiance, albedo, metallic, roughness, V, H);
}

vec3 ProcessDirectionalLight(vec3 fragPos, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness, ShadowInfo shadowInfo)
{
    vec3 V = normalize(-fragPos);
    vec3 lightDir = normalize(vec3(-light.Dir));
    vec3 H = normalize(V + lightDir);
    vec3 radiance = light.Color.rgb * light.Strength;
    // Calculate pbr contribution
    vec3 lighting = CalculateBRDF(fragNormal, lightDir, radiance, albedo, metallic, roughness, V, H);
    lighting = ComputeLightShadow(shadowInfo, fragPos, light, lighting);
    return lighting;
}

vec3 ProcessSpotLight(vec3 fragPos, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness, ShadowInfo shadowInfo)
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
        vec3 V = normalize(-fragPos);
        vec3 H = normalize(V + L);
        lighting = CalculateBRDF(fragNormal, L, radiance, albedo, metallic, roughness, V, H);
        lighting = ComputeLightShadow(shadowInfo, fragPos, light, lighting);
    }
    return lighting * intensity;
}
