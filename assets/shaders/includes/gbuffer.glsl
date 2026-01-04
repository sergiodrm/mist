
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
};

#ifdef CAMERA_DATA_INV_PROJECTION
vec3 GBuffer_ReprojectPosition(vec2 texCoords, float depth)
{
	vec4 projectedPosVS = CAMERA_DATA_INV_PROJECTION * vec4(texCoords.x * 2 - 1, (texCoords.y) * 2 - 1, depth, 1.f);
	return projectedPosVS.xyz / projectedPosVS.w;
}
#endif