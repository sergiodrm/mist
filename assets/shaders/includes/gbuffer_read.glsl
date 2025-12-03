
#ifndef GBUFFER_NORMAL_TEX
#error Must define in/out macros to read/write from textures
#endif

#ifndef GBUFFER_ALBEDO_TEX
#error Must define in/out macros to read/write from textures
#endif

#ifndef GBUFFER_EMISSIVE_TEX
#error Must define in/out macros to read/write from textures
#endif

#ifndef GBUFFER_DEPTH_TEX
#error Must define GBUFFER_DEPTH_TEX
#endif

#include <shaders/includes/gbuffer.glsl>

GBuffer GBuffer_Read(vec2 uv)
{
    GBuffer data;

    vec4 gbufferNormal = texture(GBUFFER_NORMAL_TEX, uv);
	vec4 albedo = texture(GBUFFER_ALBEDO_TEX, uv);
    vec4 emissive = texture(GBUFFER_EMISSIVE_TEX, uv);

	data.normal = normalize(gbufferNormal.xyz);
    data.metallic = emissive.a;
    data.roughness = gbufferNormal.a;
    data.albedo = albedo.rgb;
    data.opacity = albedo.a;
    data.emissive = emissive.rgb;

    return data;
}

float GBuffer_ReadDepth(vec2 texCoords)
{
    float d = texture(GBUFFER_DEPTH_TEX, texCoords).x;
    return d;
}
