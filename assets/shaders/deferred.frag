#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out vec4 outColor;

#define MAX_SHADOW_MAPS 3
layout(set = 0, binding = 1) uniform ShadowMapInfo
{
    mat4 LightViewMat[MAX_SHADOW_MAPS];
} u_ShadowMapInfo;

#include <shaders/includes/camera.glsl>
layout(set = 0, binding = 2) uniform CameraInfo
{
    Camera data;
} u_camera;
#define CAMERA_DATA u_camera.data

// GBuffer textures
layout(set = 1, binding = 0) uniform sampler2D u_GBufferPosition;
layout(set = 1, binding = 1) uniform sampler2D u_GBufferNormal;
layout(set = 1, binding = 2) uniform sampler2D u_GBufferAlbedo;
layout(set = 1, binding = 3) uniform sampler2D u_GBufferEmissive;
// SSAO texture
layout(set = 1, binding = 4) uniform sampler2D u_ssao;
// Shadow map textures
layout(set = 1, binding = 5) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];
layout(set = 1, binding = 6) uniform sampler2D u_GBufferDepth;
// Irradiance map
layout(set = 2, binding = 0) uniform samplerCube u_irradianceMap;
layout(set = 2, binding = 1) uniform samplerCube u_prefilterMap;
layout(set = 2, binding = 2) uniform sampler2D u_brdfMap;

//#define LIGHTING_NO_SHADOWS
#define LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX //u_ShadowMapInfo.LightViewMat
#define LIGHTING_SHADOWS_TEXTURE_ARRAY u_ShadowMap
#define ENVIRONMENT_DATA u_env.data
#define IRRADIANCE_MAP u_irradianceMap
#define PREFILTERED_MAP u_prefilterMap
#define BRDF_MAP u_brdfMap
#include <shaders/includes/environment_data.glsl>

#define GBUFFER_POSITION_TEX u_GBufferPosition
#define GBUFFER_NORMAL_TEX u_GBufferNormal
#define GBUFFER_ALBEDO_TEX u_GBufferAlbedo
#define GBUFFER_EMISSIVE_TEX u_GBufferEmissive
#include <shaders/includes/gbuffer_read.glsl>

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
    GBuffer data = ReadMRT(inTexCoords);
    float ao = texture(u_ssao, inTexCoords).r;
    //if (data.opacity <= 0.1f)
    //    discard;
    vec4 lightingColor = main_PBR(data.position, data.normal, data.albedo, data.metallic, data.roughness, ao);
    lightingColor.rgb += data.emissive;
#if defined(DEFERRED_APPLY_FOG)
    vec4 c = vec4(0.1, 0.1, 0.1, 1.f);
    outColor = ApplyFog(lightingColor, c);
#else
    outColor = lightingColor;
#endif // DEFERRED_APPLY_FOG
    return;
}