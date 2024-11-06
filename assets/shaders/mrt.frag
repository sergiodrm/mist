#version 450


layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in mat3 inTBN;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outEmissive;

layout(set = 2, binding = 0) uniform sampler2D u_Textures[6];
layout(set = 3, binding = 0) uniform MaterialParams
{
	vec4 Emissive;
	bool UseNormalMap;
	ivec3 Pad;
} u_material;

//#define MaterialHasMetallic(m) int(m.Params.x)
//#define MaterialHasRouhness(m) int(m.Params.y)
#define MaterialMetallic(m) m.Params.x
#define MaterialRoughness(m) m.Params.y

#define TEXTURE_ALBEDO u_Textures[0]
#define TEXTURE_NORMAL u_Textures[1]
#define TEXTURE_SPECULAR u_Textures[2]
#define TEXTURE_OCCLUSSION u_Textures[3]
#define TEXTURE_METALLIC_ROUGHNESS u_Textures[4]
#define TEXTURE_EMISSIVE u_Textures[5]


void main() 
{
	outPosition = vec4(inWorldPos, 1.0);
	outPosition.w = texture(TEXTURE_METALLIC_ROUGHNESS, inUV).b;

#define USE_TBN
#ifdef USE_TBN
	vec3 tnorm = vec3(0.f);
	if (u_material.UseNormalMap)
	{
		tnorm = inTBN * normalize(texture(TEXTURE_NORMAL, inUV).xyz * 2.0 - vec3(1.0));
	}
	else
	{
		tnorm = normalize(inNormal);
	}
	outNormal = vec4(tnorm, 1.0);
	outNormal.w = texture(TEXTURE_METALLIC_ROUGHNESS, inUV).g;
	//outNormal = normalize(texture(u_Textures[1], inUV));
#else
	//outNormal = vec4(inTangent, 1.0);
	outNormal = normalize(vec4(inNormal, 1.0));
#endif

	outEmissive = vec4(u_material.Emissive.xyz * u_material.Emissive.w, 1.f);

	outAlbedo = u_material.Emissive.w * vec4(u_material.Emissive.xyz, 1.f) + texture(TEXTURE_ALBEDO, inUV);
	if (outAlbedo.a <= 0.1)
		discard;
}