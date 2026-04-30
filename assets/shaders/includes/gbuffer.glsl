
//#define GBUFFER_NORMAL_UNORM

struct GBuffer
{
	vec3 normal;
	float roughness;

	vec3 emissive;
	float opacity;

	vec3 albedo;
	float metallic;

	vec3 padding;
	float specular;

	vec2 motionVectors;
};

GBuffer GBuffer_Zero()
{
	GBuffer gb;
	gb.normal = vec3(0.f);
	gb.roughness = 0.f;
	gb.metallic = 0.f;
	gb.emissive = vec3(0.f);
	gb.opacity = 0.f;
	gb.albedo = vec3(0.f);
	gb.specular = 0.f;
	gb.motionVectors = vec2(0.f);
	return gb;
}

#ifdef CAMERA_DATA_INV_PROJECTION
vec3 GBuffer_ReprojectPosition(vec2 texCoords, float depth)
{
	vec4 projectedPosVS = CAMERA_DATA_INV_PROJECTION * vec4(texCoords.x * 2 - 1, (texCoords.y) * 2 - 1, depth, 1.f);
	return projectedPosVS.xyz / projectedPosVS.w;
}
#endif

// Transforms from [-1,1] to [0,1] if GBuffer Normal format is UNORM
vec3 GBuffer_EncodeNormal(vec3 normal)
{
#if defined(GBUFFER_NORMAL_UNORM)
	return normal * 0.5f + 0.5f;
#else
	return normal;
#endif
}

// Transforms from [0,1] texture format UNORM to [-1,1] if needed
vec3 GBuffer_DecodeNormal(vec3 normal)
{
#if defined(GBUFFER_NORMAL_UNORM)
	return normal * 2.f - 1.f;
#else
	return normal;
#endif
}
