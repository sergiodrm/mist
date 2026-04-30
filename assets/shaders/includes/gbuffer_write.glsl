
#include <shaders/includes/gbuffer.glsl>

void GBuffer_Write(GBuffer data)
{
	outGBufferNormal.rgb = GBuffer_EncodeNormal(data.normal);
	outGBufferNormal.a = data.roughness;

	outGBufferAlbedo.rgb = data.albedo;
	outGBufferAlbedo.a = data.opacity;

	outGBufferEmissive.rgb = data.emissive;
	outGBufferEmissive.a = data.metallic;

	outGBufferSpecular.rgb = vec3(0.f, data.roughness, data.metallic);
	outGBufferSpecular.a = data.specular;

	outGBufferMotionVectors.rg = data.motionVectors;
}
