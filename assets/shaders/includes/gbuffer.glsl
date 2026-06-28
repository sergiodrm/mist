
//#define GBUFFER_NORMAL_UNORM

#ifdef _GBUFFER_READ_
// GBuffer textures
layout(set = GBUFFER_TEXTURES_SET, binding = 0) uniform sampler2D u_GBufferSpecular;
layout(set = GBUFFER_TEXTURES_SET, binding = 1) uniform sampler2D u_GBufferNormal;
layout(set = GBUFFER_TEXTURES_SET, binding = 2) uniform sampler2D u_GBufferAlbedo;
layout(set = GBUFFER_TEXTURES_SET, binding = 3) uniform sampler2D u_GBufferEmissive;
layout(set = GBUFFER_TEXTURES_SET, binding = 4) uniform sampler2D u_GBufferMotionVectors;
layout(set = GBUFFER_TEXTURES_SET, binding = 5) uniform sampler2D u_GBufferDepth;
#endif

#ifdef _GBUFFER_WRITE_
layout (location = 0) out vec4 outGBufferNormal;
layout (location = 1) out vec4 outGBufferAlbedo;
layout (location = 2) out vec4 outGBufferEmissive;
layout (location = 3) out vec4 outGBufferSpecular;
layout (location = 4) out vec4 outGBufferMotionVectors;
#endif

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

#ifdef CAMERA_DATA_INV_VIEW_PROJECTION
// Returns World Position
vec3 GBuffer_ReprojectPosition(vec2 texCoords, float depth)
{
	vec4 projectedPosVS = CAMERA_DATA_INV_VIEW_PROJECTION * vec4(texCoords.x * 2 - 1, (texCoords.y) * 2 - 1, depth, 1.f);
	return projectedPosVS.xyz / projectedPosVS.w;
}
#endif // CAMERA_DATA_INV_VIEW_PROJECTION

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

vec2 GBuffer_ComputeMotionVectors(vec4 posWS_0, vec4 posWS_1)
{
	// Motion vectors
	vec2 currPos = (posWS_0.xy / posWS_0.w) * 0.5 + 0.5;
	vec2 prevPos = (posWS_1.xy / posWS_1.w) * 0.5 + 0.5;
	return (currPos - prevPos);
}
