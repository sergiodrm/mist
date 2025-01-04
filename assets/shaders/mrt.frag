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



void main() 
{
	outPosition = vec4(inWorldPos, 1.0);
	outPosition.w = texture(u_Textures[TEXTURE_METALLIC_ROUGHNESS], inUV).b;

	vec3 tnorm = vec3(0.f);
	if (bool(u_material.data.Flags & MATERIAL_FLAG_HAS_NORMAL_MAP))
	{
		tnorm = inTBN * normalize(texture(u_Textures[TEXTURE_NORMAL], inUV).xyz * 2.0 - vec3(1.0));
	}
	else
	{
		tnorm = normalize(inNormal);
	}
	outNormal = vec4(tnorm, 1.0);
	outNormal.w = texture(u_Textures[TEXTURE_METALLIC_ROUGHNESS], inUV).g;

	outEmissive = vec4(u_material.data.Emissive.xyz * u_material.data.Emissive.w, 1.f);
	outAlbedo = u_material.data.Emissive.w * vec4(u_material.data.Emissive.xyz, 1.f) + texture(u_Textures[TEXTURE_ALBEDO], inUV);

	if (outAlbedo.a <= 0.1)
		discard;
}