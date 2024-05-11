#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out float fragColor;

#define KERNEL_SIZE 64

int KernelSize = KERNEL_SIZE;

// GBuffer textures
layout(set = 0, binding = 0) uniform SSAOUniform
{
    vec2 NoiseScale;
    float Radius;
    vec3 Samples[KERNEL_SIZE];
    mat4 Projection;
} u_ssao;

layout(set = 0, binding = 1) uniform sampler2D u_GBufferPosition;
layout(set = 0, binding = 2) uniform sampler2D u_GBufferNormal;
layout(set = 0, binding = 3) uniform sampler2D u_GBufferAlbedo;
layout(set = 0, binding = 4) uniform sampler2D u_SSAONoise;


void main()
{
	vec3 fragPos = texture(u_GBufferPosition, inTexCoords).xyz;
    vec3 normal = texture(u_GBufferNormal, inTexCoords).xyz;
    vec3 randomVec = texture(u_SSAONoise, inTexCoords * u_ssao.NoiseScale).rgb;
    vec3 tangent = normalize(randomVec -normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.f;
    float bias = 0.025;
    float radius = u_ssao.Radius;
    for (int i = 0; i < KernelSize; ++i)
    {
        vec3 samplePos = TBN * u_ssao.Samples[i];
        samplePos = fragPos * samplePos * radius;

        vec4 offset = vec4(samplePos, 1.f);
        offset = u_ssao.Projection * offset; // view to clip space
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;
        float sampleDepth = texture(u_GBufferPosition, offset.xy).z;
        float rangeCheck = smoothstep(0.f, 1.f, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.f : 0.f) * rangeCheck;
    }
    occlusion = 1.f - (occlusion / KernelSize);
    fragColor = occlusion;
    //fragColor = fragPos.x;
}