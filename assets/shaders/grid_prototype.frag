#version 450



layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in vec4 inCurrWSPos;
layout (location = 6) in vec4 inPrevWSPos;
layout (location = 7) in mat3 inTBN;

layout (location = 0) out vec4 outGBufferNormal;
layout (location = 1) out vec4 outGBufferAlbedo;
layout (location = 2) out vec4 outGBufferEmissive;
layout (location = 3) out vec4 outGBufferSpecular;
layout (location = 4) out vec4 outGBufferMotionVectors;

layout(set = 2, binding = 0) uniform sampler2D u_Textures[6];

#include <shaders/includes/material.glsl>
layout(set = 2, binding = 1) uniform MaterialBlock
{
	MaterialUniformBuffer data;
} u_material;

#include <shaders/includes/gbuffer_write.glsl>

float ComputeGridLine(float threshold, float worldOffset, vec3 pos)
{
	vec3 f = fract(worldOffset+pos);
	vec3 infLim = 1.f - smoothstep(0.f, threshold, f);
	vec3 supLim = smoothstep(1.f - threshold, 1.0f, f);
	vec3 s = infLim+supLim;
	return s.x+s.y+s.z;
}


void main() 
{
	GBuffer data = GBuffer_Zero();
#if 0
	vec2 x = 1-smoothstep(vec2(0.495f), vec2(0.505f), fract(inWorldPos.xz / 1.f));
	float sq = 1-x.x-x.y+2*x.x*x.y;
	vec3 color = vec3(1,0,0);
	data.albedo = vec3(sq*color);
#elif 1
	float mainLine = ComputeGridLine(0.02f, 0.f, inWorldPos);
	float secondLine = ComputeGridLine(0.009f, 0.5f, inWorldPos);

	const vec3 backgroundColor = vec3(0.9f);
	const vec3 lineColor = vec3(0.1);
	const vec3 secondLineColor = vec3(0.2f);
	data.albedo = mainLine*lineColor + (1.f-mainLine)*backgroundColor;
	data.albedo = (1-secondLine)*data.albedo + secondLine*secondLineColor;
#endif

	data.opacity = 1.f;

	// Albedo and emissive
	data.opacity = u_material.data.Albedo.a;
	data.emissive = u_material.data.Emissive.w * u_material.data.Emissive.rgb;

 	// Normals
	data.normal = normalize(inNormal);

	// Metallic and Roughness
	data.roughness = 1.f;//u_material.data.MetallicRoughness.g;
	data.metallic = 0.f;//u_material.data.MetallicRoughness.r;

	// Specular
	data.specular = 0.f;//u_material.data.MetallicRoughness.b;

	// Motion vectors
	vec2 currPos = (inCurrWSPos.xy / inCurrWSPos.w) * 0.5 + 0.5;
	vec2 prevPos = (inPrevWSPos.xy / inPrevWSPos.w) * 0.5 + 0.5;
	data.motionVectors = (currPos - prevPos);

	GBuffer_Write(data);
}