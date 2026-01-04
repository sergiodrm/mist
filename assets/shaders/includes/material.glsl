

struct MaterialParams
{
	vec4 Emissive; // w: emissive strength
	vec3 Albedo;
	float Specular;
	vec4 MetallicRoughness; // zw: padding
	ivec4 Flags; // yzw: padding
};
