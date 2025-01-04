
#include <shaders/includes/math.glsl>

/*
 * Data structures for lighting
*/

struct LightData
{
    // Point light -> Pos: position
    // Directional light -> Pos : direction
    vec4 Pos; // w: pointlight-radius; directional-project shadows (-1.f not project. >=0.f shadow map index)
    vec4 Color; // w: compression
};

struct SpotLightData
{
    vec4 Color; // w: project shadows (-1.f not project. >=0.f shadow map index)
    vec4 Dir; // w: inner cutoff
    vec4 Pos; // w: outer cutoff
    // x: radius, y: compression, zw: unused (padding)
    vec4 Params;
};

/*
 * Data structures for lighting
*/
#ifndef LIGHTING_NO_SHADOWS
#define LIGHTING_SHADOWS_PCF

#ifndef LIGHTING_SHADOWS_TEXTURE_ARRAY
#error Macro LIGHTING_SHADOWS_TEXTURE_ARRAY must be define to calculate shadow value
#endif // !LIGHTING_SHADOWS_TEXTURE_ARRAY

#ifndef MAX_SHADOW_MAPS
#error Define num of shadow maps
#endif // !MAX_SHADOW_MAPS

struct ShadowInfo
{
#ifndef LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    vec4 ShadowCoordArray[MAX_SHADOW_MAPS];
#else
    mat4 LightViewMatrices[MAX_SHADOW_MAPS];
#endif // !LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
};

#endif // !LIGHTING_NO_SHADOWS

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

vec3 CalculateBRDF(vec3 normal, vec3 lightDir, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 viewDir, vec3 halfway)
{
    vec3 F0 = vec3(0.04);
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

float ComputeShadow(ShadowInfo info, vec3 fragPos, int shadowIndex)
{
    // Calculate shadow coordinate from ShadowInfo (LightViewMatrix or precalculated fragPos into light space).
#ifndef LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    vec4 shadowCoord = info.ShadowCoordArray[shadowIndex];
#else
    vec4 shadowCoord = info.LightViewMatrices[shadowIndex] * vec4(fragPos, 1.f);
#endif // !LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX
    shadowCoord = shadowCoord / shadowCoord.w;

    // Shadow calculation
#if defined(LIGHTING_SHADOWS_PCF)
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(LIGHTING_SHADOWS_TEXTURE_ARRAY[shadowIndex], 0);
    float currentDepth = shadowCoord.z;
    float bias = 0.005f;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(LIGHTING_SHADOWS_TEXTURE_ARRAY[shadowIndex], shadowCoord.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
#else
    float shadow = 0.3f;
    float z = shadowCoord.z;
	if ( z > -1.0 && z < 1.0 ) 
	{
		float dist = texture( u_ShadowMap[0], shadowCoord.xy ).r;
        const float bias = 0.005f;
		if ( shadowCoord.w > 0.0 && z - bias > dist ) 
		{
			shadow = 1.f;
		}
	}
	return shadow;
#endif // defined(LIGHTING_SHADOWS_PCF)
}

float ComputeLightShadow(ShadowInfo info, vec3 fragPos, LightData light)
{
    float shadow = 0.f;
    if (light.Pos.w >= 0.f)
    {
        shadow = ComputeShadow(info, fragPos, int(light.Pos.w));
    }
    return shadow;
}

float ComputeSpotLightShadow(ShadowInfo info, vec3 fragPos, SpotLightData light)
{
    float shadow = 0.f;
    if (light.Color.w >= 0.f)
    {
        shadow = ComputeShadow(info, fragPos, int(light.Color.w));
    }
    return shadow;
}

#endif // !LIGHTING_NO_SHADOWS

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

/**
 * Light types calculations
*/

float ComputeSpotLightIntensity(SpotLightData light, vec3 fragPos)
{
    vec3 lightDir = normalize(vec3(light.Pos) - fragPos);
    float theta = dot(lightDir, -vec3(light.Dir));
    float cosCutoff = light.Dir.a;
    float cosOuterCutoff = light.Pos.a;
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
    float radius = LightRadius(light);
    float compression = LightCompression(light);
    float attenuation = CalculateAttenuation(distance, radius, compression);
    vec3 radiance = light.Color.rgb * attenuation;

    return CalculateBRDF(fragNormal, L, radiance, albedo, metallic, roughness, V, H);
}

vec3 ProcessDirectionalLight(vec3 fragPos, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness, ShadowInfo shadowInfo)
{
    vec3 V = normalize(-fragPos);
    vec3 lightDir = normalize(vec3(-light.Pos));
    vec3 H = normalize(V + lightDir);
    vec3 radiance = light.Color.rgb;
    // Calculate pbr contribution
    vec3 lighting = CalculateBRDF(fragNormal, lightDir, radiance, albedo, metallic, roughness, V, H);
#ifndef LIGHTING_NO_SHADOWS
    // Calculate shadow contribution
    float shadow = ComputeLightShadow(shadowInfo, fragPos, light);
    lighting *= (1.f-shadow);
#endif // !LIGHTING_NO_SHADOWS
    return lighting;
}

vec3 ProcessSpotLight(vec3 fragPos, vec3 fragNormal, SpotLightData light, vec3 albedo, float metallic, float roughness, ShadowInfo shadowInfo)
{
    vec3 lighting = vec3(0.f);
    float intensity = ComputeSpotLightIntensity(light, fragPos);
    if (intensity > 0.f)
    {
        float radius = light.Params.x;
        float compression = light.Params.y;
        float distance = length(light.Pos.xyz - fragPos);
        float attenuation = CalculateAttenuation(distance, radius, compression);
        vec3 radiance = light.Color.rgb * attenuation;
        vec3 L = normalize(light.Pos.xyz - fragPos);
        vec3 V = normalize(-fragPos);
        vec3 H = normalize(V + L);
        lighting = CalculateBRDF(fragNormal, L, radiance, albedo, metallic, roughness, V, H);
#ifndef LIGHTING_NO_SHADOWS
        // Calculate shadow inside lighting cone
        float shadow = ComputeSpotLightShadow(shadowInfo, fragPos, light);
        lighting *= (1.f-shadow);
#endif // !LIGHTING_NO_SHADOWS
    }
    return lighting * intensity;
}
