#version 460

#include <shaders/includes/math.glsl>

layout (location = 0) out vec4 fragColor;
layout (location = 0) in vec3 localPos;
  
layout (set = 1, binding = 0) uniform samplerCube u_cubemap;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}
  
void main()
{
    vec3 normal = normalize(localPos);
    vec3 irradiance = vec3(0.0);
    vec3 up = vec3(0,1,0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.f;
    for (float phi = 0.f; phi < 2.f * M_PI; phi += sampleDelta)
    {
        for (float theta = 0.f; theta < 0.5 * M_PI; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += texture(u_cubemap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = M_PI * irradiance * 1.f / float(nrSamples);

    fragColor = vec4(irradiance, 1.f);
}