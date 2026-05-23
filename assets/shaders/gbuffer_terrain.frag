#version 450

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) in vec3 inWorldPos;
layout (location = 2) in vec4 inCurrWSPos;
layout (location = 3) in vec4 inPrevWSPos;

layout (location = 0) out vec4 outGBufferNormal;
layout (location = 1) out vec4 outGBufferAlbedo;
layout (location = 2) out vec4 outGBufferEmissive;
layout (location = 3) out vec4 outGBufferSpecular;
layout (location = 4) out vec4 outGBufferMotionVectors;

layout(set = 1, binding = 0) uniform sampler2D u_Textures[6];

#define MATERIAL_DISABLE_TEXTURES
#include <shaders/includes/material.glsl>
layout(set = 1, binding = 1) uniform MaterialBlock
{
	MaterialUniformBuffer data;
} u_material;

#include <shaders/includes/gbuffer_write.glsl>

void main() 
{
	MaterialPBR pbrData = Material_ReadPBR(u_material.data, vec3(0,1,0));

	GBuffer data;
	data.opacity = pbrData.opacity;
#ifdef ALPHA_TEST
	// Do alpha test
	//if (Material_DoAlphaTest(data.opacity, u_material.data.MetallicRoughness.a))
	//	discard;
#endif
	data.albedo = pbrData.albedo;
	data.normal = pbrData.normal;
	data.emissive = pbrData.emissive;
	data.roughness = pbrData.roughness;
	data.metallic = pbrData.metallic;
	data.specular = pbrData.specular;
	data.motionVectors = GBuffer_ComputeMotionVectors(inCurrWSPos, inPrevWSPos);
	data.roughness = 1.f;
	data.metallic = 0.f;

	vec3 snowColor = vec3(1.f, 1.f, 1.f);
	vec3 floorColor = vec3(0.0362f, 0.0212f, 0.0017f);
	data.albedo = mix(floorColor, snowColor, (inWorldPos.y+16.f)/64.f);
	//data.albedo = vec3((inWorldPos.y+16.f)/64.f, 0,0);
	GBuffer_Write(data);
}