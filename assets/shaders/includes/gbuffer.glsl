
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