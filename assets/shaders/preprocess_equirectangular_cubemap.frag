#version 460

layout (location = 0) out vec4 fragColor;
layout (location = 0) in vec3 localPos;
  
layout (set = 1, binding = 0) uniform sampler2D u_map;

layout (set = 2, binding = 0) uniform DataBlock
{
    vec3 clampColorMin;
    vec3 clampColorMax;
} u_data;

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
    vec2 uv = SampleSphericalMap(normalize(localPos));
    vec3 c = texture(u_map, uv).rgb;
    c = clamp(c, u_data.clampColorMin, u_data.clampColorMax);
    fragColor = vec4(c, 1.f);
}