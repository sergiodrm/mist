#version 460

layout (set = 0, binding = 0) uniform sampler2D u_hdrtex;
layout (set = 1, binding = 0) uniform UBO 
{
    float GammaCorrection;
    float Exposure;
} u_HdrParams;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 InTexCoords;

void main()
{
    const float gammaCorrection = u_HdrParams.GammaCorrection;
    vec4 hdrColor = texture(u_hdrtex, InTexCoords);

    // reinhard tone mapping
    //vec3 mapped = hdrColor.xyz / (hdrColor.xyz + vec3(1.f));
    vec3 mapped = vec3(1.f) - exp(-hdrColor.rgb * u_HdrParams.Exposure);
    // gamme correction
    mapped = pow(mapped, vec3(1.f / gammaCorrection));

    FragColor = vec4(mapped, hdrColor.a);
    //FragColor = vec4(hdrColor.rgb, hdrColor.a);
}
