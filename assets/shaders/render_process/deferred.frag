#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out vec4 outColor;

#define _CAMERA_NOT_PREV_FRAME_
#include <shaders/includes/camera.glsl>

#define CAMERA_DATA_INV_VIEW_PROJECTION u_camera.data.invViewProjection
#define GBUFFER_TEXTURES_SET 1
#define LIGHTING_DATA_SET 2
#define LIGHTING_TEXTURES_SET 3
#include <shaders/includes/gbuffer_read.glsl>
#include <shaders/includes/lighting.glsl>

// SSAO texture
layout(set = 1, binding = 6) uniform sampler2D u_ssao;
// Shadow map textures

vec3 ApplyFog(vec3 lightingColor, vec3 fogColor)
{
    float d = LinearizeDepth(texture(u_GBufferDepth, inTexCoords).r, 1.f, 1000.f);
#if 1 // linear interpolation
    return mix(lightingColor, fogColor, d);
#elif 1 // custom interp
    //return (1.f-d*d)*lightingColor+d*d*fogColor;
    float f = 0.1f*exp(d);
    return (1.f-f)*lightingColor+f*fogColor;
#endif
}

void main()
{
    GBuffer data = GBuffer_Read(inTexCoords);
    float depth = GBuffer_ReadDepth(inTexCoords);
    vec3 posWS = GBuffer_ReprojectPosition(inTexCoords, depth);
    float ao = texture(u_ssao, inTexCoords).r;
    vec3 viewWorldPos = u_camera.data.invView[3].xyz;
    vec3 viewDir = -normalize(posWS - viewWorldPos);

    //data.albedo += data.emissive;
    vec3 lightingColor = DoLighting(posWS, viewDir, data.normal, data.albedo, data.metallic, data.roughness, ao);
    lightingColor.rgb += data.emissive;

#if defined(DEFERRED_APPLY_FOG)
    vec3 c = vec3(0.1, 0.1, 0.1);
    lightingColor = ApplyFog(lightingColor, c);
#endif // DEFERRED_APPLY_FOG

    outColor = vec4(lightingColor, 1.f);

}