

struct MaterialUniformBuffer
{
	vec4 Emissive; // w: emissive strength
	vec4 Albedo; // w: unused

	// r: metallic, g: roughness, b: specular, a: alpha cutoff
	vec4 MetallicRoughness; 

	ivec4 Flags; // yzw: padding
};


