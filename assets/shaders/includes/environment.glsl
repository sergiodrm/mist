

#ifndef ENVIRONMENT_DATA
#error Must define ENVIRONMENT_DATA macro
#endif

//#define DEBUG_AMBIENT
//#define DEBUG_LIGHTS 

#define ENV_APPLY_GI

/// DoEnvironmentLighting
/// * fragPos: fragment position in world space
/// * viewDir: view direction -> normalize(fragWorldPos - viewWorldPos)
/// * normal: fragment normal in world space
vec3 DoEnvironmentLighting(vec3 fragPos, vec3 viewDir, vec3 normal, vec3 albedo, float metallic, float roughness, float ao, ShadowInfo shadowInfo)
{
    // Point lights
    vec3 pointLightsColor = vec3(0.f);
    for (int i = 0; i < ENVIRONMENT_DATA.numOfPointLights; ++i)
    {
        pointLightsColor += ProcessPointLight(fragPos, viewDir, normal, ENVIRONMENT_DATA.pointLights[i], albedo, metallic, roughness);
    }

    // Spot lights
    vec3 spotLightsColor = vec3(0.f);
    for (int i = 0; i < ENVIRONMENT_DATA.numOfSpotLights; ++i)
    {
        spotLightsColor += ProcessSpotLight(fragPos, viewDir, normal, ENVIRONMENT_DATA.spotLights[i], albedo, metallic, roughness, shadowInfo);
    }

    // Directional light
    vec3 directionalLightColor = vec3(0.f);
    for (int i = 0; i < ENVIRONMENT_DATA.numOfDirectionalLights; ++i)
    {
        directionalLightColor += ProcessDirectionalLight(fragPos, viewDir, normal, ENVIRONMENT_DATA.directionalLights[i], albedo, metallic, roughness, shadowInfo);
    }

    vec3 lightColor = (pointLightsColor + spotLightsColor + directionalLightColor);

    // Ambient color
#ifdef ENV_APPLY_GI
    vec3 N = normal;
    vec3 V = viewDir;
    vec3 ambientColor = ENVIRONMENT_DATA.ambientColor * ProcessIrradiance(N, V, albedo, roughness, metallic, ao);
#else
    vec3 ambientColor = ENVIRONMENT_DATA.ambientColor * albedo * ao;
#endif
#if defined(DEBUG_AMBIENT)
	return ambientColor;
#elif defined(DEBUG_LIGHTS)
    return lightColor;
#else
    return lightColor + ambientColor;
#endif
}
