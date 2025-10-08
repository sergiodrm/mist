#version 450

#include <shaders/includes/material.glsl>


layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in mat3 inTBN;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outEmissive;

layout(set = 2, binding = 0) uniform sampler2D u_Textures[6];
layout(set = 3, binding = 0) uniform MaterialBlock
{
	MaterialParams data;
} u_material;


#define GBUFFER_POSITION_TEX outPosition
#define GBUFFER_NORMAL_TEX outNormal
#define GBUFFER_ALBEDO_TEX outAlbedo
#define GBUFFER_EMISSIVE_TEX outEmissive
#include <shaders/includes/gbuffer_write.glsl>


void main() 
{
	GBuffer data;

	// Positions
	data.position = inWorldPos;
	
	// Normals
	if (bool(u_material.data.Flags.x & MATERIAL_FLAG_HAS_NORMAL_MAP))
		data.normal = inTBN * normalize(texture(u_Textures[MATERIAL_TEXTURE_NORMAL], inUV).xyz * 2.0 - vec3(1.0));
	else
		data.normal = normalize(inNormal);
	// Metallic and Roughness
	if (bool(u_material.data.Flags.x & MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP))
	{
		vec3 mr = texture(u_Textures[MATERIAL_TEXTURE_METALLIC_ROUGHNESS], inUV).rgb;
		data.roughness = mr.g * u_material.data.MetallicRoughness.g;
		data.metallic = mr.b * u_material.data.MetallicRoughness.r;
	}
	else
	{
		data.roughness = u_material.data.MetallicRoughness.g;
		data.metallic = u_material.data.MetallicRoughness.r;
	}

	// Albedo and emissive
	vec4 albedo = texture(u_Textures[MATERIAL_TEXTURE_ALBEDO], inUV);
	data.albedo = albedo.rgb * u_material.data.Albedo.rgb;
	data.opacity = albedo.a;
	data.emissive = u_material.data.Emissive.w * u_material.data.Emissive.rgb;

	WriteMRT(data);
}