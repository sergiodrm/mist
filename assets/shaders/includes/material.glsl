

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

// Texture access
#define TEXTURE_ALBEDO 				0
#define TEXTURE_NORMAL 				1
#define TEXTURE_SPECULAR 			2
#define TEXTURE_OCCLUSSION 			3
#define TEXTURE_METALLIC_ROUGHNESS 	4
#define TEXTURE_EMISSIVE 			5

// Material flags
#define MATERIAL_FLAG_HAS_NORMAL_MAP 				0x01
#define MATERIAL_FLAG_HAS_METALLIC_ROUGHNESS_MAP 	0x02
#define MATERIAL_FLAG_HAS_SPECULAR_GLOSSINESS_MAP 	0x04
#define MATERIAL_FLAG_HAS_EMISSIVE_MAP 				0x08
#define MATERIAL_FLAG_EMISSIVE 						0x10
#define MATERIAL_FLAG_UNLIT 						0x20
#define MATERIAL_FLAG_NO_PROJECT_SHADOWS 			0x40

