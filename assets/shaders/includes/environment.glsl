

#ifndef ENVIRONMENT_DATA
#error Must define ENVIRONMENT_DATA macro
#endif

vec3 DoEnvironmentLighting(vec3 fragPos, vec3 normal, vec3 albedo, float metallic, float roughness, float ao, ShadowInfo shadowInfo)
{
    // Point lights
    vec3 pointLightsColor = vec3(0.f);
    for (int i = 0; i < ENVIRONMENT_DATA.NumOfPointLights; ++i)
    {
        pointLightsColor += ProcessPointLight(fragPos, normal, ENVIRONMENT_DATA.Lights[i], albedo, metallic, roughness);
    }

    // Spot lights
    vec3 spotLightsColor = vec3(0.f);
    for (int i = 0; i < ENVIRONMENT_DATA.NumOfSpotLights; ++i)
    {
        spotLightsColor += ProcessSpotLight(fragPos, normal, ENVIRONMENT_DATA.SpotLights[i], albedo, metallic, roughness, shadowInfo);
    }

    // Directional light
    vec3 directionalLightColor = ProcessDirectionalLight(fragPos, normal, ENVIRONMENT_DATA.DirectionalLight, albedo, metallic, roughness, shadowInfo);

    vec3 lightColor = (pointLightsColor + spotLightsColor + directionalLightColor);

    // Ambient color
    vec3 N = normalize(mat3(CAMERA_DATA.invView) * normal);
    vec4 wspos = ((CAMERA_DATA.invView) * vec4(fragPos, 1));
    vec3 pos = (wspos/wspos.w).xyz;
    vec3 camPos = (CAMERA_DATA.invView)[3].xyz;
    vec3 V = normalize(camPos - pos);
    //return N;
    //return V;

    vec3 ambientColor = ENVIRONMENT_DATA.AmbientColor * ProcessIrradiance(N, V, albedo, roughness, metallic, ao);
	//return ambientColor;
    return lightColor + ambientColor;
}
