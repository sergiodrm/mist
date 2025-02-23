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

layout(set = 2, binding = 0) uniform sampler2D u_Textures[6];
layout(set = 3, binding = 0) uniform MaterialBlock
{
	MaterialParams data;
} u_material;



void main() 
{
	// Positions
	outPosition.rgb = inWorldPos;
	
	// Normals
	if (bool(u_material.data.Flags & MATERIAL_FLAG_HAS_NORMAL_MAP))
		outNormal.rgb = inTBN * normalize(texture(u_Textures[MATERIAL_TEXTURE_NORMAL], inUV).xyz * 2.0 - vec3(1.0));
	else
		outNormal.rgb = normalize(inNormal);
	// Metallic and Roughness
	if (bool(u_material.data.Flags & MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP))
	{
		vec3 mr = texture(u_Textures[MATERIAL_TEXTURE_METALLIC_ROUGHNESS], inUV).rgb;
		outNormal.a = mr.g * u_material.data.Roughness;
		outPosition.a = mr.b * u_material.data.Metallic;
	}
	else
	{
		outNormal.a = u_material.data.Roughness;
		outPosition.a = u_material.data.Metallic;
	}

	// Albedo and emissive
	outAlbedo = texture(u_Textures[MATERIAL_TEXTURE_ALBEDO], inUV);
	outAlbedo.rgb *= u_material.data.Albedo.rgb;
	outAlbedo.rgb += u_material.data.Emissive.w * u_material.data.Emissive.rgb;

	if (outAlbedo.a <= 0.1)
		discard;
}