#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 1) uniform ShadowMapInfo
{
    mat4 LightViewMat[MAX_SHADOW_MAPS];
    vec4 noiseParams; // w: padding
    vec4 textureOffset3DSize; // xyz: tex3D offset size. w: filter radius
    vec4 textureOffsetParams; // x: window size. y: filter size. zw: shadow map texture size
} u_ShadowMapInfo;

#define LIGHTING_SHADOWS_NOISE_SCALE u_ShadowMapInfo.noiseParams.x
#define LIGHTING_SHADOWS_NOISE_FACTOR u_ShadowMapInfo.noiseParams.y
#define LIGHTING_SHADOWS_NOISE_PCF_KERNEL_SIZE int(u_ShadowMapInfo.noiseParams.z)

#define LIGHTING_SHADOWS_TEXTURE_OFFSET_3D_SIZE u_ShadowMapInfo.textureOffset3DSize.xyz
#define LIGHTING_SHADOWS_TEXTURE_FILTER_RADIUS u_ShadowMapInfo.noiseParams.w
#define LIGHTING_SHADOWS_TEXTURE_WINDOW_SIZE int(u_ShadowMapInfo.textureOffsetParams.x)
#define LIGHTING_SHADOWS_TEXTURE_FILTER_SIZE int(u_ShadowMapInfo.textureOffsetParams.y)

#define LIGHTING_SHADOWS_SHADOW_MAP_SIZE u_ShadowMapInfo.textureOffsetParams.zw

#define _CAMERA_NOT_PREV_FRAME_
#include <shaders/includes/camera.glsl>
#define CAMERA_DATA u_camera.data

// GBuffer textures
layout(set = 1, binding = 0) uniform sampler2D u_GBufferSpecular;
layout(set = 1, binding = 1) uniform sampler2D u_GBufferNormal;
layout(set = 1, binding = 2) uniform sampler2D u_GBufferAlbedo;
layout(set = 1, binding = 3) uniform sampler2D u_GBufferEmissive;
layout(set = 1, binding = 4) uniform sampler2D u_GBufferMotionVectors;
// SSAO texture
layout(set = 1, binding = 5) uniform sampler2D u_ssao;
// Shadow map textures
layout(set = 1, binding = 6) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];
layout(set = 1, binding = 7) uniform sampler2D u_GBufferDepth;
// Irradiance map
layout(set = 2, binding = 0) uniform samplerCube u_irradianceMap;
layout(set = 2, binding = 1) uniform samplerCube u_prefilterMap;
layout(set = 2, binding = 2) uniform sampler2D u_brdfMap;

layout(set = 2, binding = 3) uniform sampler2D u_blueNoise;
layout(set = 2, binding = 4) uniform sampler3D u_offset3D;

//#define LIGHTING_NO_SHADOWS
#define LIGHTING_SHADOWS_LIGHT_VIEW_MATRIX //u_ShadowMapInfo.LightViewMat
#define LIGHTING_SHADOWS_TEXTURE_ARRAY u_ShadowMap
#define ENVIRONMENT_DATA u_env.data
#define IRRADIANCE_MAP u_irradianceMap
#define PREFILTERED_MAP u_prefilterMap
#define BRDF_MAP u_brdfMap
#include <shaders/includes/environment_data.glsl>

#define GBUFFER_NORMAL_TEX u_GBufferNormal
#define GBUFFER_ALBEDO_TEX u_GBufferAlbedo
#define GBUFFER_EMISSIVE_TEX u_GBufferEmissive
#define GBUFFER_DEPTH_TEX u_GBufferDepth
#define CAMERA_DATA_INV_PROJECTION (CAMERA_DATA.invViewProjection)
#include <shaders/includes/gbuffer_read.glsl>

layout (std140, set = 3, binding = 0) uniform EnvBlock
{
    Environment data;
} u_env;
#include <shaders/includes/environment.glsl>

vec4 main_PBR(vec3 worldPos, vec3 normal, vec3 albedo, float metallic, float roughness, float ao)
{
    ShadowInfo shadowInfo;
    shadowInfo.LightViewMatrices[0] = u_ShadowMapInfo.LightViewMat[0];
    shadowInfo.LightViewMatrices[1] = u_ShadowMapInfo.LightViewMat[1];
    shadowInfo.LightViewMatrices[2] = u_ShadowMapInfo.LightViewMat[2];

    vec3 viewWorldPos = CAMERA_DATA.invView[3].xyz;
    vec3 viewDir = -normalize(worldPos - viewWorldPos);
    
    vec3 color = DoEnvironmentLighting(worldPos, viewDir, normal, albedo, metallic, roughness, ao, shadowInfo);
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
    GBuffer data = GBuffer_Read(inTexCoords);
    float depth = GBuffer_ReadDepth(inTexCoords);
    vec3 posWS = GBuffer_ReprojectPosition(inTexCoords, depth);
    float ao = texture(u_ssao, inTexCoords).r;
    data.albedo += data.emissive;
    vec4 lightingColor = main_PBR(posWS, data.normal, data.albedo, data.metallic, data.roughness, ao);
    lightingColor.rgb += data.emissive;
#if defined(DEFERRED_APPLY_FOG)
    vec4 c = vec4(0.1, 0.1, 0.1, 1.f);
    outColor = ApplyFog(lightingColor, c);
#else
    outColor = lightingColor;
#endif // DEFERRED_APPLY_FOG
//outColor = vec4(gl_FragCoord.xyz / vec3(1920, 1080, 1), 1.f);
}