#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out float fragColor;

#ifndef KERNEL_SIZE
#define KERNEL_SIZE 64
#endif

//int KernelSize = KERNEL_SIZE;

// GBuffer textures
layout(set = 0, binding = 0) uniform SSAOUniform
{
    vec4 Params;
    mat4 Projection;
    mat4 InverseProjection;
    vec4 Samples[KERNEL_SIZE];
} u_ssao;

#define ssao_radius u_ssao.Params.x
#define ssao_bias   u_ssao.Params.y
#define ssao_bypass (u_ssao.Params.z == 0.f)

//layout(set=1, binding=0) uniform sampler2D u_textures[5];
//layout(set = 0, binding = 1) uniform sampler2D u_GBufferPosition;
//layout(set = 0, binding = 2) uniform sampler2D u_GBufferNormal;
//layout(set = 0, binding = 3) uniform sampler2D u_GBufferAlbedo;
//layout(set = 0, binding = 4) uniform sampler2D u_DepthBuffer;
//layout(set = 0, binding = 5) uniform sampler2D u_SSAONoise;

layout (set = 1, binding = 0) uniform sampler2D u_GBufferPosition;
layout (set = 2, binding = 0) uniform sampler2D u_GBufferNormal;
layout (set = 3, binding = 0) uniform sampler2D u_SSAONoise;

vec3 GetPosVSFromDepth(float depth, vec2 uv)
{
    float x = uv.x * 2.f - 1.f;
    //float y = (1.f-uv.y) * 2.f - 1.f;
    float y = (uv.y) * 2.f - 1.f;
    vec4 pos = vec4(x, y, depth, 1.f);
    vec4 posVS = u_ssao.InverseProjection * pos;
    return posVS.xyz / posVS.w;
}

float linearDepth(float depth, float near, float far)
{
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * near * far) / (far + near - z * (far - near));	
}

void main()
{
    if (ssao_bypass)
    {
        fragColor = 1.f;
        return;
    }

    // Current fragment 
	vec3 fragPos = texture(u_GBufferPosition, inTexCoords).xyz;
    vec3 normal = normalize(texture(u_GBufferNormal, inTexCoords).xyz);

    // Calculate uvcoords for noise texture
    ivec2 texDim = textureSize(u_GBufferNormal, 0); 
	ivec2 noiseDim = textureSize(u_SSAONoise, 0);
    vec2 noiseScale = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/float(noiseDim.y));
    vec2 noiseUV = noiseScale * inTexCoords;
    vec3 randomVec = texture(u_SSAONoise, noiseUV).xyz * 2.0 - 1.0;

    // Calculate TBN matrix with noise applied
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.f;
    float bias = ssao_bias;
    float radius = ssao_radius;
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        // calculate sample pos in view space
        vec3 samplePos = TBN * u_ssao.Samples[i].xyz;
        samplePos = fragPos + samplePos * radius;

        // transform view space to clip space
        vec4 offset = vec4(samplePos, 1.f);
        offset = u_ssao.Projection * offset; // view to clip space
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;
        vec2 uv = offset.xy;

        // get depth from samplePos in clip space
        float sampleDepth = texture(u_GBufferPosition, uv).z;
        float rangeCheck = smoothstep(0.f, 1.f, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.f : 0.f) * rangeCheck;
    }
    occlusion = 1.f - (occlusion / float(KERNEL_SIZE));
    fragColor = occlusion;
    //fragColor = linearDepth(fragPos.z, 1.f, 1000.f);
}