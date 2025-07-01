

struct MaterialParams
{
	vec4 Emissive; // w: emissive strength
	vec4 Albedo; // w: unused
	vec4 MetallicRoughness; // zw: padding
	ivec4 Flags; // yzw: padding
};
