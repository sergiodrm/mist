#version 460


// Color output
layout(location = 0) out vec4 outColor;

// Vertex shader input
layout(location = 0) in vec4 inFragPosWS;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;
layout(location = 4) in vec4 inLightSpaceFragPos_0;
layout(location = 5) in vec4 inLightSpaceFragPos_1;
layout(location = 6) in vec4 inLightSpaceFragPos_2;
layout(location = 7) in vec3 inViewDir;
layout(location = 8) in mat3 inTBN;

// Uniforms
#ifndef MAX_SHADOW_MAPS
#error Must define MAX_SHADOW_MAPS value
#endif // MAX_SHADOW_MAPS

// Shadow mapping
layout (set = 2, binding = 0) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];
// Irradiance maps
layout(set = 2, binding = 1) uniform samplerCube u_irradianceMap;
layout(set = 2, binding = 2) uniform samplerCube u_prefilterMap;
layout(set = 2, binding = 3) uniform sampler2D u_brdfMap;
layout(set = 2, binding = 4) uniform samplerCube u_cubemap;
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
    if (MATERIAL_HAS_NORMAL_MAP(u_material.data.Flags.x))
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

vec4 DoLighting(vec3 fragPos, vec3 viewDir, vec3 normal, vec3 albedo, float opacity, float metallic, float roughness)
{
    ShadowInfo shadowInfo;
    shadowInfo.ShadowCoordArray[0] = inLightSpaceFragPos_0;
    shadowInfo.ShadowCoordArray[1] = inLightSpaceFragPos_1;
    shadowInfo.ShadowCoordArray[2] = inLightSpaceFragPos_2;
    
    vec3 lightColor = DoEnvironmentLighting(fragPos, viewDir, normal, albedo, metallic, roughness, 1.f, shadowInfo);
    vec4 color = vec4(lightColor, opacity);

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
    outColor = vec4(1,0,0,0.2);
    MaterialPBR mtl = Material_ReadPBR(u_material.data, inTexCoords, inNormal);
    
    vec4 color = DoLighting(inFragPosWS.xyz, inViewDir, mtl.normal, mtl.albedo, mtl.opacity, mtl.metallic, mtl.roughness);
    outColor = color;
}
