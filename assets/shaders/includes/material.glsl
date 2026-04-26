

struct MaterialUniformBuffer
{
	vec4 Emissive; // w: emissive strength
	vec4 Albedo; // w: unused

	// r: metallic, g: roughness, b: specular, a: alpha cutoff
	vec4 MetallicRoughness; 

	// x: 16 left bits flags, 16 right bits textureMask
	ivec4 Flags; // yzw: padding
};

#define _MATERIAL_HAS_MAP(_flags, _map) (bool(_flags.x & (1<<_map)))
#define MATERIAL_HAS_ALBEDO_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_ALBEDO)
#define MATERIAL_HAS_NORMAL_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_NORMAL)
#define MATERIAL_HAS_SPECULAR_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_SPECULAR)
#define MATERIAL_HAS_OCCLUSION_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_OCCLUSION)
#define MATERIAL_HAS_METALLIC_ROUGHNESS_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_METALLIC_ROUGHNESS)
#define MATERIAL_HAS_EMISSIVE_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_EMISSIVE)

