#version 460


// Color output
layout(location = 0) out vec4 outColor;

// Vertex shader input
layout(location = 0) in vec4 inFragPosVS;
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

// Shadow mapping
layout (set = 2, binding = 0) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];
// SSAO
layout (set = 2, binding = 1) uniform sampler2D u_ssao;
// Irradiance maps
layout(set = 2, binding = 2) uniform samplerCube u_irradianceMap;
layout(set = 2, binding = 3) uniform samplerCube u_prefilterMap;
layout(set = 2, binding = 4) uniform sampler2D u_brdfMap;
layout(set = 2, binding = 5) uniform samplerCube u_cubemap;
// Material maps
layout(set = 3, binding = 0) uniform sampler2D u_Textures[6];

#define LIGHTING_NO_SHADOWS
#define LIGHTING_SHADOWS_TEXTURE_ARRAY u_ShadowMap
#define ENVIRONMENT_DATA u_env.data
#define IRRADIANCE_MAP u_irradianceMap
#define PREFILTERED_MAP u_prefilterMap
#define BRDF_MAP u_brdfMap
#include <shaders/includes/environment_data.glsl>
#include <shaders/includes/material.glsl>

// Per frame data
layout (std140, set = 4, binding = 0) uniform EnvBlock
{
    Environment data;
} u_env;
#include <shaders/includes/environment.glsl>

layout (set = 5, binding = 0) uniform MaterialBlock
{
    MaterialUniformBuffer data;
} u_material;


vec3 ComputeNormalMapping(vec3 normal)
{
    if (bool(u_material.data.Flags.x & MATERIAL_FLAG_HAS_NORMAL_MAP))
    {
        vec3 normalValue = texture(u_Textures[MATERIAL_TEXTURE_NORMAL], inTexCoords).rgb;
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
    
    vec3 lightColor = DoEnvironmentLighting(fragPos, normal, albedo.rgb, metallic, roughness, 1.f, shadowInfo);
    vec4 color = vec4(lightColor, albedo.a);

//#ifdef CUBEMAP_REFLECTION
//    // Cubemap reflection
//    vec3 reflection = ComputeCubemapReflection(fragPos, normal, vec3(0.f));
//    color = vec4(mix(color.rgb, reflection, 0.02f*metallic), color.a);
//#endif // CUBEMAP_REFLECTION
//#ifdef DEBUG_SPECULAR
//    color = vec4(lightColor, 1.f);
//#endif // DEBUG_SPECULAR
    return color;
}

void main()
{
    // Albedo
    vec4 albedo = u_material.data.Albedo;
    if (bool(u_material.data.Flags.x & MATERIAL_FLAG_HAS_ALBEDO_MAP))
        albedo *= texture(u_Textures[MATERIAL_TEXTURE_ALBEDO], inTexCoords);

#ifndef UNLIT

    // Normal
    vec3 normal = ComputeNormalMapping(inNormal);

    // MetallicRoughness
	float roughness = u_material.data.MetallicRoughness.g;
	float metallic = u_material.data.MetallicRoughness.r;
	if (bool(u_material.data.Flags.x & MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP))
	{
		vec3 mr = texture(u_Textures[MATERIAL_TEXTURE_METALLIC_ROUGHNESS], inTexCoords).rgb;
		roughness *= mr.g;
		metallic *= mr.b;
	}

    outColor = DoLighting(inFragPosVS.xyz, normal, albedo, metallic, roughness);
    //outColor = vec4(0,0,0,0);
    //outColor.r = albedo.a;
#else
    outColor = albedo;
#endif // !UNLIT

	outColor += u_material.data.Emissive.w * vec4(u_material.data.Emissive.xyz, 1.f);
#ifdef EMISSIVE
    //outEmissive = vec4(u_material.data.Emissive.xyz * u_material.data.Emissive.w, 1.f);
#else
    //outEmissive = vec4(0.f);
#endif // EMISSIVE
}
