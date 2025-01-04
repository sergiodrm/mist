#version 460


// Color output
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outEmissive;

// Vertex shader input
layout(location = 0) in vec4 inFragPosWS;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;
layout(location = 4) in vec4 inLightSpaceFragPos_0;
layout(location = 5) in vec4 inLightSpaceFragPos_1;
layout(location = 6) in vec4 inLightSpaceFragPos_2;
layout(location = 7) in mat3 inTBN;

// Uniforms
#ifndef MAX_SHADOW_MAPS
#error Must define MAX_SHADOW_MAPS value
#endif // MAX_SHADOW_MAPS

layout (set = 2, binding = 0) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];
//layout (set = 0, binding = 4) uniform sampler2D u_SSAOTex;

layout(set = 3, binding = 0) uniform sampler2D u_Textures[6];

#define LIGHTING_SHADOWS_TEXTURE_ARRAY u_ShadowMap
#include <shaders/includes/environment.glsl>
#include <shaders/includes/material.glsl>

// Per frame data
layout (std140, set = 4, binding = 0) uniform EnvBlock
{
    Environment data;
} u_env;

layout (set = 5, binding = 0) uniform MaterialBlock
{
    MaterialParams data;
} u_material;

layout (set = 6, binding = 0) uniform samplerCube u_cubemap;

vec3 ComputeNormalMapping(vec3 normal)
{
    if (bool(u_material.data.Flags & MATERIAL_FLAG_HAS_NORMAL_MAP))
    {
        vec3 normalValue = texture(u_Textures[TEXTURE_NORMAL], inTexCoords).rgb;
        normalValue = normalize(normalValue*2.f - vec3(1.f));
        return normalize(inTBN * normalValue);
    }
    else
    {
        return normalize(normal);
    }
}

vec3 ComputeCubemapReflection(vec3 fragPos, vec3 normal, vec3 viewPos)
{
    vec3 I = normalize(fragPos - viewPos);
    vec3 R = reflect(I, normalize(normal));
    return texture(u_cubemap, R).rgb;
}

vec4 DoLighting(vec3 fragPos, vec3 normal, vec4 albedo, float metallic, float roughness)
{
    ShadowInfo shadowInfo;
    shadowInfo.ShadowCoordArray[0] = inLightSpaceFragPos_0;
    shadowInfo.ShadowCoordArray[1] = inLightSpaceFragPos_1;
    shadowInfo.ShadowCoordArray[2] = inLightSpaceFragPos_2;
    
    // Point lights
    int numPointLights = int(u_env.data.ViewPos.w);
    vec3 pointLightsColor = vec3(0.f);
    for (int i = 0; i < numPointLights; ++i)
    {
        pointLightsColor += ProcessPointLight(fragPos, normal, u_env.data.Lights[i], albedo.rgb, metallic, roughness);
    }

    // Spot lights
    int numSpotLights = int(u_env.data.AmbientColor.w);
    vec3 spotLightsColor = vec3(0.f);
    for (int i = 0; i < numSpotLights; ++i)
    {
        spotLightsColor += ProcessSpotLight(fragPos, normal, u_env.data.SpotLights[i], albedo.rgb, metallic, roughness, shadowInfo);
    }

    // Directional light
    vec3 directionalLightColor = ProcessDirectionalLight(fragPos, normal, u_env.data.DirectionalLight, albedo.rgb, metallic, roughness, shadowInfo);

    vec3 lightColor = (pointLightsColor + spotLightsColor + directionalLightColor);

    // Ambient color
#if 0
    vec2 texSize = textureSize(u_SSAOTex, 0);
    vec2 uv = gl_FragCoord.xy / texSize;
    vec3 ambientColor = vec3(u_env.data.AmbientColor) * texture(u_SSAOTex, uv).r;
#else
    vec3 ambientColor = vec3(u_env.data.AmbientColor);
#endif


    // Mix
    vec4 color = vec4(lightColor, 1.f) + albedo*vec4(ambientColor, 1.f);
#ifdef CUBEMAP_REFLECTION
    // Cubemap reflection
    vec3 reflection = ComputeCubemapReflection(fragPos, normal, vec3(0.f));
    color = vec4(mix(color.rgb, reflection, 0.02f*metallic), color.a);
#endif // CUBEMAP_REFLECTION
#ifdef DEBUG_SPECULAR
    color = vec4(lightColor, 1.f);
#endif // DEBUG_SPECULAR
    return color;
}

void main()
{
    vec4 albedo = texture(u_Textures[TEXTURE_ALBEDO], inTexCoords);
    if (albedo.a <= 0.1f)
        discard;

#ifndef UNLIT
#ifndef EMISSIVE
    vec3 normal = ComputeNormalMapping(inNormal);
    vec4 metallicRoughness = texture(u_Textures[TEXTURE_METALLIC_ROUGHNESS], inTexCoords);
    float metallic = metallicRoughness.b;
    float roughness = metallicRoughness.g;
    outColor = DoLighting(inFragPosWS.xyz, normal, albedo, metallic, roughness);
#endif // !EMISSIVE
#else
    outColor = albedo;
#endif // !UNLIT

#ifdef EMISSIVE
	outColor = u_material.data.Emissive.w * vec4(u_material.data.Emissive.xyz, 1.f) + albedo;
    outEmissive = vec4(u_material.data.Emissive.xyz * u_material.data.Emissive.w, 1.f);
#else
    outEmissive = vec4(0.f);
#endif // EMISSIVE
}
