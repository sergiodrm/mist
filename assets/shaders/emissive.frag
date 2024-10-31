#version 460

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 inFragPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;

layout(set = 2, binding = 0) uniform sampler2D u_Textures[6];
layout(set = 3, binding = 0) uniform MaterialParams
{
	vec4 Emissive;
} u_material;


#define TEXTURE_ALBEDO u_Textures[0]
#define TEXTURE_NORMAL u_Textures[1]
#define TEXTURE_SPECULAR u_Textures[2]
#define TEXTURE_OCCLUSSION u_Textures[3]
#define TEXTURE_METALLIC_ROUGHNESS u_Textures[4]
#define TEXTURE_EMISSIVE u_Textures[5]

void main()
{
    outColor = texture(TEXTURE_EMISSIVE, inTexCoords);
    outColor.xyz = u_material.Emissive.xyz;
    outColor.a = 1.f;
    outColor.xyz *= u_material.Emissive.a;
}