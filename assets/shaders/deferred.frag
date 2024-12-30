#version 460



layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out vec4 outColor;

#define MAX_SHADOW_MAPS 3
layout(set = 0, binding = 1) uniform ShadowMapInfo
{
    mat4 LightViewMat[MAX_SHADOW_MAPS];
} u_ShadowMapInfo;

// GBuffer textures
layout(set = 1, binding = 0) uniform sampler2D u_GBufferPosition;
layout(set = 2, binding = 0) uniform sampler2D u_GBufferNormal;
layout(set = 3, binding = 0) uniform sampler2D u_GBufferAlbedo;
// SSAO texture
layout(set = 4, binding = 0) uniform sampler2D u_ssao;
// Shadow map textures
layout(set = 5, binding = 0) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];
layout(set = 6, binding = 0) uniform sampler2D u_GBufferDepth;

#define LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX //u_ShadowMapInfo.LightViewMat
#define LIGHTING_SHADOWS_TEXTURE_ARRAY u_ShadowMap
#define LIGHTING_SHADOWS_PCF
#include "shaders/includes/lighting.glsl"


// Per frame data
layout (std140, set = 0, binding = 0) uniform Environment
{
    vec4 AmbientColor; // w: num of spot lights to process
    vec4 ViewPos; // w: num of lights to process
    LightData Lights[8];
    LightData DirectionalLight;
    SpotLightData SpotLights[8];
} u_Env;







vec4 main_PBR(vec3 FragViewPos, vec3 Normal, vec3 Albedo, float Metallic, float Roughness, float AO)
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(-FragViewPos);
    vec3 Lo = vec3(0.f);

    ShadowInfo shadowInfo;
    shadowInfo.LightViewMatrices[0] = u_ShadowMapInfo.LightViewMat[0];
    shadowInfo.LightViewMatrices[1] = u_ShadowMapInfo.LightViewMat[1];
    shadowInfo.LightViewMatrices[2] = u_ShadowMapInfo.LightViewMat[2];

    // Point lights
    int numPointLights = int(u_Env.ViewPos.w);
    for (int i = 0; i < numPointLights; ++i)
    {
        LightData Light = u_Env.Lights[i];
        Lo += ProcessPointLight(FragViewPos, Normal, Light, Albedo, Metallic, Roughness);
    }

    // Spot lights
    int numSpotLights = int(u_Env.AmbientColor.w);
    vec3 spotLightColor = vec3(0.f);
    for (int i = 0; i < numSpotLights; ++i)
    {
        spotLightColor += ProcessSpotLight(FragViewPos, N, u_Env.SpotLights[i], Albedo, Metallic, Roughness, shadowInfo);
    }
    Lo += spotLightColor;
    //return vec4(spotLightColor, 1.f);

    // Directional light
    vec3 directionalLightColor = ProcessDirectionalLight(FragViewPos, N, u_Env.DirectionalLight, Albedo, Metallic, Roughness, shadowInfo);
    Lo += directionalLightColor;

    vec3 Ambient = vec3(u_Env.AmbientColor) * Albedo * AO;
    vec3 Color = Ambient + Lo;
    //Color = Color / (Color + vec3(1.f));
    //Color = pow(Color, vec3(1.f/2.2f));

    return vec4(Color, 1.f);
}

#if defined(DEFERRED_APPLY_FOG)
vec4 ApplyFog(vec4 lightingColor, vec4 fogColor)
{
    float d = LinearizeDepth(texture(u_GBufferDepth, inTexCoords).r, 1.f, 1000.f);
#if 1 // linear interpolation
    return mix(lightingColor, fogColor, d);
#elif 1 // custom interp
    //return (1.f-d*d)*lightingColor+d*d*fogColor;
    float f = 0.1f*exp(d);
    return (1.f-f)*lightingColor+f*fogColor;
#endif
}
#endif // DEFERRED_APPLY_FOG

void main()
{
#if 1
    vec4 gbufferPos = texture(u_GBufferPosition, inTexCoords);
    vec4 gbufferNormal = texture(u_GBufferNormal, inTexCoords);
	vec3 fragPos = gbufferPos.rgb;
	vec3 fragNormal = normalize(gbufferNormal.xyz);
	vec4 fragColor = texture(u_GBufferAlbedo, inTexCoords);
    float metallic = gbufferPos.a;
    float roughness = gbufferNormal.a;
    float ao = texture(u_ssao, inTexCoords).r;
    if (fragColor.a <= 0.1f)
        discard;
    vec4 lightingColor = main_PBR(fragPos, fragNormal, fragColor.rgb, metallic, roughness, ao);
#if defined(DEFERRED_APPLY_FOG)
    vec4 c = vec4(0.1, 0.1, 0.1, 1.f);
    outColor = ApplyFog(lightingColor, c);
#else
    outColor = lightingColor;
#endif // DEFERRED_APPLY_FOG
    return;
#else // non pbr

	vec3 fragPos = texture(u_GBufferPosition, inTexCoords).rgb;
	vec3 fragNormal = normalize(texture(u_GBufferNormal, inTexCoords).rgb);
	vec4 fragColor = texture(u_GBufferAlbedo, inTexCoords);
	vec3 viewPos = vec3(0.f);//u_Env.ViewPos.xyz;
    outColor = fragColor;
    //outColor = vec4(fragPos, 1.0);
    //return;
    if (fragColor.a <= 0.1f)
        discard;

    vec3 lightColor = vec3(1.f, 1.f, 1.f);
    
    {
        // Shadows

        // Point lights (no shadows)
        int numPointLights = int(u_Env.ViewPos.w);
        vec3 pointLightsColor = vec3(0.f);
        for (int i = 0; i < numPointLights; ++i)
            pointLightsColor += ProcessPointLight(fragPos, fragNormal, viewPos, u_Env.Lights[i]);

        // Spot lights
        int numSpotLights = int(u_Env.AmbientColor.w);
        vec3 spotLightsColor = vec3(0.f);
        float spotLightShadow = 0.f;
        for (int i = 0; i < numSpotLights; ++i)
        {
            vec3 spotLightColor = ProcessSpotLight(fragPos, fragNormal, viewPos, u_Env.SpotLights[i]);
#if 0
            if (u_Env.SpotLights[i].Color.w >= 0.f)
            {
                int shadowIndex = int(u_Env.SpotLights[i].Color.w);
                float shadow = CalculateShadow(vec4(fragPos, 1.f), shadowIndex);
                spotLightColor *= (1.f-shadow);
            }
#endif
            spotLightsColor += spotLightColor;
        }
        spotLightsColor *= (1.f-spotLightShadow);
        // Directional light
        vec3 directionalLightColor = ProcessDirectionalLight(fragPos, fragNormal, viewPos, u_Env.DirectionalLight);
#if 0
        if (u_Env.DirectionalLight.Pos.w >= 0.f)
        {
            int shadowIndex = int(u_Env.DirectionalLight.Pos.w);
            float shadow = CalculateShadow(shadowIndex);
            directionalLightColor *= (1.f - shadow);
        }
#endif

        lightColor = (pointLightsColor + spotLightsColor + directionalLightColor);
        // Mix
        vec3 ambientColor = vec3(u_Env.AmbientColor) * texture(u_ssao, inTexCoords).r;
        lightColor = ambientColor + lightColor;
        //lightColor = vec3(shadow);
        //lightColor = vec3(shadowCoord);
    }
    outColor = vec4(lightColor, 1.f) * outColor;
#endif
}