

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

void WriteMRT(GBuffer data)
{
	GBUFFER_POSITION_TEX.rgb = data.position;
	GBUFFER_POSITION_TEX.a = data.metallic;

	GBUFFER_NORMAL_TEX.rgb = data.normal;
	GBUFFER_NORMAL_TEX.a = data.roughness;

	GBUFFER_ALBEDO_TEX.rgb = data.albedo;
	GBUFFER_ALBEDO_TEX.a = data.opacity;

	GBUFFER_EMISSIVE_TEX.rgb = data.emissive;
	GBUFFER_EMISSIVE_TEX.a = 0.f;
}
