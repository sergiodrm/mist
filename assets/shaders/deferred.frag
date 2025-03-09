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
#define ENVIRONMENT_DATA u_env.data
#include <shaders/includes/environment_data.glsl>

layout (std140, set = 0, binding = 0) uniform EnvBlock
{
    Environment data;
} u_env;
#include <shaders/includes/environment.glsl>

vec4 main_PBR(vec3 FragViewPos, vec3 Normal, vec3 Albedo, float Metallic, float Roughness, float AO)
{
    ShadowInfo shadowInfo;
    shadowInfo.LightViewMatrices[0] = u_ShadowMapInfo.LightViewMat[0];
    shadowInfo.LightViewMatrices[1] = u_ShadowMapInfo.LightViewMat[1];
    shadowInfo.LightViewMatrices[2] = u_ShadowMapInfo.LightViewMat[2];
    
    vec3 color = DoEnvironmentLighting(FragViewPos, Normal, Albedo, Metallic, Roughness, AO, shadowInfo);
    return vec4(color, 1.f);
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
}