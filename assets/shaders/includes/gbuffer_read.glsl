

#ifndef GBUFFER_POSITION_TEX
#error Must define in/out macros to read/write from textures
#endif

#ifndef GBUFFER_NORMAL_TEX
#error Must define in/out macros to read/write from textures
#endif

#ifndef GBUFFER_ALBEDO_TEX
#error Must define in/out macros to read/write from textures
#endif

#ifndef GBUFFER_EMISSIVE_TEX
#error Must define in/out macros to read/write from textures
#endif

#include <shaders/includes/gbuffer.glsl>

GBuffer ReadMRT(vec2 uv)
{
    GBuffer data;

    vec4 gbufferPos = texture(GBUFFER_POSITION_TEX, uv);
    vec4 gbufferNormal = texture(GBUFFER_NORMAL_TEX, uv);
	vec4 albedo = texture(GBUFFER_ALBEDO_TEX, uv);

	data.position = gbufferPos.rgb;
	data.normal = normalize(gbufferNormal.xyz);
    data.metallic = gbufferPos.a;
    data.roughness = gbufferNormal.a;
    data.albedo = albedo.rgb;
    data.opacity = albedo.a;
    data.emissive = texture(GBUFFER_EMISSIVE_TEX, uv).rgb;

    return data;
}
