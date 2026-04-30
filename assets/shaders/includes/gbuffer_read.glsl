
#include <shaders/includes/gbuffer.glsl>

GBuffer GBuffer_Read(vec2 uv)
{
    GBuffer data;

    vec4 gbufferNormal = texture(u_GBufferNormal, uv);
	vec4 albedo = texture(u_GBufferAlbedo, uv);
    vec4 emissive = texture(u_GBufferEmissive, uv);
    vec4 specular = texture(u_GBufferSpecular, uv);
    vec2 motionVectors = texture(u_GBufferMotionVectors, uv).rg;

	data.normal = GBuffer_DecodeNormal(normalize(gbufferNormal.xyz));
    data.metallic = specular.b;
    data.roughness = specular.g;
    data.albedo = albedo.rgb;
    data.opacity = albedo.a;
    data.emissive = emissive.rgb;
    data.specular = specular.w;
    data.motionVectors = motionVectors;

    return data;
}

float GBuffer_ReadDepth(vec2 texCoords)
{
    float d = texture(u_GBufferDepth, texCoords).x;
    return d;
}
