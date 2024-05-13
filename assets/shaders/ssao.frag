#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out float fragColor;

#define KERNEL_SIZE 64

int KernelSize = KERNEL_SIZE;

// GBuffer textures
layout(set = 0, binding = 0) uniform SSAOUniform
{
    vec3 NoiseScale;
    float Radius;
    vec4 Samples[KERNEL_SIZE];
    mat4 Projection;
} u_ssao;

layout(set = 0, binding = 1) uniform sampler2D u_GBufferPosition;
layout(set = 0, binding = 2) uniform sampler2D u_GBufferNormal;
layout(set = 0, binding = 3) uniform sampler2D u_GBufferAlbedo;
layout(set = 0, binding = 4) uniform sampler2D u_DepthBuffer;
layout(set = 0, binding = 5) uniform sampler2D u_SSAONoise;

vec3 GetPosVSFromDepth(vec2 uv, mat4 invProj)
{
    float depth = -texture(u_DepthBuffer, uv).r;
    float x = uv.x * 2.f - 1.f;
    //float y = (1.f-uv.y) * 2.f - 1.f;
    float y = (uv.y) * 2.f - 1.f;
    vec4 pos = vec4(x, y, depth, 1.f);
    vec4 posVS = invProj * pos;
    return posVS.xyz / posVS.w;
}

float linearDepth(float depth, float near, float far)
{
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * near * far) / (far + near - z * (far - near));	
}

void main()
{
	//vec3 fragPos = texture(u_GBufferPosition, inTexCoords).xyz;
    vec3 fragPos = GetPosVSFromDepth(inTexCoords, inverse(u_ssao.Projection));
    vec3 normal = normalize(texture(u_GBufferNormal, inTexCoords).xyz);
    ivec2 texDim = textureSize(u_GBufferNormal, 0); 
	ivec2 noiseDim = textureSize(u_SSAONoise, 0);
    vec2 noiseScale = vec2(texDim.x/noiseDim.x, texDim.y/noiseDim.y);
    vec3 randomVec = texture(u_SSAONoise, inTexCoords * noiseScale).rgb;
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    //tangent = vec3(normal.x, -normal.z, normal.y);
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.f;
    float bias = 0.025;
    //float radius = u_ssao.Radius;
    float radius = 0.5f;
    for (int i = 0; i < KernelSize; ++i)
    {
        vec3 samplePos = TBN * u_ssao.Samples[i].xyz;
        samplePos = fragPos + samplePos * radius;

        vec4 offset = vec4(samplePos, 1.f);
        offset = u_ssao.Projection * offset; // view to clip space
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;
        //float sampleDepth = texture(u_GBufferPosition, offset.xy).b;
        float sampleDepth = GetPosVSFromDepth(offset.xy, inverse(u_ssao.Projection)).z;
        float rangeCheck = smoothstep(0.f, 1.f, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.f : 0.f) * rangeCheck;
    }
    occlusion = 1.f - (occlusion / float(KernelSize));
    fragColor = occlusion;
    //fragColor = linearDepth(fragPos.z, 1.f, 1000.f);
}