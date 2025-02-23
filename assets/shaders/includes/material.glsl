

struct MaterialParams
{
	vec4 Emissive; // w: emissive strength
	vec4 Albedo; // w: unused
	float Metallic;
	float Roughness;
	vec2 _padding;
	int Flags;
	ivec3 _padding2;
};
