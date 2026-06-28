#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in vec4 inCurrWSPos;
layout (location = 6) in vec4 inPrevWSPos;
layout (location = 7) in mat3 inTBN;

#define CAMERA_DATA_INV_VIEW_PROJECTION (u_camera.data.invViewProjection)
#include <shaders/includes/camera.glsl>
#include <shaders/includes/material.glsl>
#include <shaders/includes/instance_data.glsl>
#include <shaders/includes/gbuffer_write.glsl>

void main() 
{
	MaterialPBR pbrData = Material_ReadPBR(u_materialBuffer.data[u_instance.data.materialId], inUV, inNormal);

	GBuffer data;
	data.opacity = pbrData.opacity;
#ifdef ALPHA_TEST
	// Do alpha test
	if (Material_DoAlphaTest(data.opacity, u_materialBuffer.data[u_instance.data.materialId].MetallicRoughness.a))
		discard;
#endif
	data.albedo = pbrData.albedo;
	data.normal = pbrData.normal;
	data.emissive = pbrData.emissive;
	data.roughness = pbrData.roughness;
	data.metallic = pbrData.metallic;
	data.specular = pbrData.specular;
	data.motionVectors = GBuffer_ComputeMotionVectors(inCurrWSPos, inPrevWSPos);
	
	GBuffer_Write(data);
}