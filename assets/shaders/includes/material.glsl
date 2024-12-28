

struct MaterialParams
{
	vec4 Emissive;
	bool UseNormalMap;
	ivec3 Pad;
};

//#define MaterialHasMetallic(m) int(m.Params.x)
//#define MaterialHasRouhness(m) int(m.Params.y)
#define MaterialMetallic(m) m.Params.x
#define MaterialRoughness(m) m.Params.y

#define TEXTURE_ALBEDO(texArray) texArray[0]
#define TEXTURE_NORMAL(texArray) texArray[1]
#define TEXTURE_SPECULAR(texArray) texArray[2]
#define TEXTURE_OCCLUSSION(texArray) texArray[3]
#define TEXTURE_METALLIC_ROUGHNESS(texArray) texArray[4]
#define TEXTURE_EMISSIVE(texArray) texArray[5]
