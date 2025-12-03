
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

void GBuffer_Write(GBuffer data)
{
	GBUFFER_NORMAL_TEX.rgb = data.normal;
	GBUFFER_NORMAL_TEX.a = data.roughness;

	GBUFFER_ALBEDO_TEX.rgb = data.albedo;
	GBUFFER_ALBEDO_TEX.a = data.opacity;

	GBUFFER_EMISSIVE_TEX.rgb = data.emissive;
	GBUFFER_EMISSIVE_TEX.a = data.metallic;
}
