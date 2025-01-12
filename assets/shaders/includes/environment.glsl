
#include <shaders/includes/lighting.glsl>

// Per frame data
struct Environment
{
    vec3 AmbientColor;
    int NumOfSpotLights;
    vec3 ViewPos;
    int NumOfPointLights;
    LightData Lights[8];
    LightData DirectionalLight;
    LightData SpotLights[8];
};

vec3 DoEnvironmentLighting(vec3 fragPos, vec3 normal, Environment env, vec3 albedo, float metallic, float roughness, ShadowInfo shadowInfo)
{
    // Point lights
    vec3 pointLightsColor = vec3(0.f);
    for (int i = 0; i < env.NumOfPointLights; ++i)
    {
        pointLightsColor += ProcessPointLight(fragPos, normal, env.Lights[i], albedo, metallic, roughness);
    }

    // Spot lights
    vec3 spotLightsColor = vec3(0.f);
    for (int i = 0; i < env.NumOfSpotLights; ++i)
    {
        spotLightsColor += ProcessSpotLight(fragPos, normal, env.SpotLights[i], albedo, metallic, roughness, shadowInfo);
    }

    // Directional light
    vec3 directionalLightColor = ProcessDirectionalLight(fragPos, normal, env.DirectionalLight, albedo, metallic, roughness, shadowInfo);

    vec3 lightColor = (pointLightsColor + spotLightsColor + directionalLightColor);

    // Ambient color
    vec3 ambientColor = vec3(env.AmbientColor);

    return lightColor + albedo * ambientColor;
}
