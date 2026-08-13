
#include <shaders/includes/math.glsl>
#include <shaders/includes/pbr.glsl>

#include <shaders/includes/shadows.glsl>
#include <shaders/includes/lighting_data.glsl>


//#define LIGHTING_DEBUG_AMBIENT
//#define LIGHTING_DEBUG_LIGHTS 
//#define LIGHTING_APPLY_GI
//#define LIGHTING_SHADOWS_DEBUG


// Irradiance map
layout(set = LIGHTING_TEXTURES_SET, binding = 0) uniform samplerCube u_irradianceMap;
layout(set = LIGHTING_TEXTURES_SET, binding = 1) uniform samplerCube u_prefilterMap;
layout(set = LIGHTING_TEXTURES_SET, binding = 2) uniform sampler2D u_brdfMap;



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

vec3 ProcessDirectionalLight(vec3 fragPos, vec3 viewDir, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness)
{
    vec3 V = normalize(viewDir);
    vec3 lightDir = normalize(vec3(-light.Dir));
    vec3 H = normalize(V + lightDir);
    vec3 radiance = light.Color.rgb * light.Strength;
    // Calculate pbr contribution
    vec3 lighting = CalculateBRDF(fragNormal, lightDir, radiance, albedo, metallic, roughness, V, H);
    lighting = ComputeLightShadow(fragPos, lightDir, fragNormal, light.ShadowMapIndex, lighting);
    return lighting;
}

vec3 ProcessSpotLight(vec3 fragPos, vec3 viewDir, vec3 fragNormal, LightData light, vec3 albedo, float metallic, float roughness)
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
        lighting = ComputeLightShadow(fragPos, L, fragNormal, light.ShadowMapIndex, lighting);
    }
    return lighting * intensity;
}

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
        spotLightsColor += ProcessSpotLight(fragPos, viewDir, normal, u_lighting.data.spotLights[i], albedo, metallic, roughness);
    }

    // Directional light
    vec3 directionalLightColor = vec3(0.f);
    for (int i = 0; i < u_lighting.data.numOfDirectionalLights; ++i)
    {
        directionalLightColor += ProcessDirectionalLight(fragPos, viewDir, normal, u_lighting.data.directionalLights[i], albedo, metallic, roughness);
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
