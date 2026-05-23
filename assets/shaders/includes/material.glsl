
#define MATERIAL_APPLY_TEXTURES
#ifdef MATERIAL_DISABLE_TEXTURES
#undef MATERIAL_APPLY_TEXTURES
#endif

struct MaterialUniformBuffer
{
	vec4 Emissive; // w: emissive strength
	vec4 Albedo; // w: unused

	// r: metallic, g: roughness, b: specular, a: alpha cutoff
	vec4 MetallicRoughness; 

	// x: 16 left bits flags, 16 right bits textureMask
	ivec4 Flags; // yzw: padding
};

#ifdef MATERIAL_APPLY_TEXTURES
#define _MATERIAL_HAS_MAP(_flags, _map) (bool(_flags.x & (1<<_map)))
#define MATERIAL_HAS_ALBEDO_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_ALBEDO)
#define MATERIAL_HAS_NORMAL_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_NORMAL)
#define MATERIAL_HAS_SPECULAR_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_SPECULAR)
#define MATERIAL_HAS_OCCLUSION_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_OCCLUSION)
#define MATERIAL_HAS_METALLIC_ROUGHNESS_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_METALLIC_ROUGHNESS)
#define MATERIAL_HAS_EMISSIVE_MAP(_flags) _MATERIAL_HAS_MAP(_flags, MATERIAL_TEXTURE_EMISSIVE)
#endif


struct MaterialPBR
{
	vec3 normal;
	vec3 albedo;
	float specular;
	float ao;
	float opacity;
	float metallic;
	float roughness;
	vec3 emissive;
};

bool Material_DoAlphaTest(float opacity, float threshold)
{
	return opacity < threshold;
}

MaterialPBR Material_ReadPBR(const MaterialUniformBuffer mtl, 
#ifdef MATERIAL_APPLY_TEXTURES
	vec2 texCoords, 
#endif
	vec3 vertexNormal)
{
	MaterialPBR data;
	// Albedo and emissive
	data.albedo = mtl.Albedo.rgb;
	data.opacity = mtl.Albedo.a;
#ifdef MATERIAL_APPLY_TEXTURES
	if (MATERIAL_HAS_ALBEDO_MAP(mtl.Flags.x))
	{
		vec4 albedo = texture(u_Textures[MATERIAL_TEXTURE_ALBEDO], texCoords);
		data.albedo *= albedo.rgb;
		data.opacity *= albedo.a;
	}
#endif
	data.emissive = mtl.Emissive.w * mtl.Emissive.rgb;

	// Normals
#ifdef MATERIAL_APPLY_TEXTURES
	if (MATERIAL_HAS_NORMAL_MAP(mtl.Flags.x))
		data.normal = inTBN * normalize(texture(u_Textures[MATERIAL_TEXTURE_NORMAL], texCoords).xyz * 2.0 - vec3(1.0));
	else
#endif
		data.normal = normalize(vertexNormal);

	// Metallic and Roughness
	data.roughness = mtl.MetallicRoughness.g;
	data.metallic = mtl.MetallicRoughness.r;
#ifdef MATERIAL_APPLY_TEXTURES
	if (MATERIAL_HAS_METALLIC_ROUGHNESS_MAP(mtl.Flags.x))
	{
		vec3 mr = texture(u_Textures[MATERIAL_TEXTURE_METALLIC_ROUGHNESS], texCoords).rgb;
		data.roughness *= mr.g;
		data.metallic *= mr.b;
	}
#endif

	// Specular
	data.specular = mtl.MetallicRoughness.b;
#ifdef MATERIAL_APPLY_TEXTURES
	if (MATERIAL_HAS_SPECULAR_MAP(mtl.Flags.x))
	{
		vec4 specular = texture(u_Textures[MATERIAL_TEXTURE_SPECULAR], texCoords);
		data.specular *= specular.a;
		data.roughness *= specular.g;
		data.metallic *= specular.b;
	}
#endif
	return data;
}

