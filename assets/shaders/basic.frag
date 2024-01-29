#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec4 inFragPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;

struct LightData
{
    vec4 Pos;
    vec4 Color;
};

layout (std140, set = 1, binding = 1) uniform Environment
{
    vec4 AmbientColor;
    vec4 ViewPos;
    LightData Lights[8];
} u_Env;

layout(set = 2, binding = 0) uniform sampler2D texArray[3];

// Debug push constants
layout( push_constant ) uniform constants
{
	int ForcedTexIndex;
	int DrawDebug;
    int EnableLighting;
} PushConstants;

vec3 ProcessLight(LightData light)
{
    vec3 normal = normalize(inNormal);
    vec3 lightDir = -normalize(vec3(inFragPos) - vec3(light.Pos));
    float diff = max(dot(normal, lightDir), 0.f);
    vec4 diffuse = diff * light.Color;
    return vec3(diffuse);
}

void main()
{
    if (PushConstants.ForcedTexIndex > -1)
    {
        outColor = texture(texArray[PushConstants.ForcedTexIndex], inTexCoords);
    }
    else
    {
        outColor = texture(texArray[0], inTexCoords);
        vec3 lightColor = vec3(1.f, 1.f, 1.f);
        if (PushConstants.EnableLighting != 0)
        {
            lightColor = vec3(0.f, 0.f, 0.f);
            for (int i = 0; i < 1; ++i)
                lightColor += ProcessLight(u_Env.Lights[i]);
            lightColor = vec3(u_Env.AmbientColor) + lightColor;
        }
        outColor = vec4(lightColor, 1.f) * outColor;
    }
    if (PushConstants.DrawDebug == 1)
    {
        outColor = vec4(inNormal, 1.f);
    } 
    else if (PushConstants.DrawDebug == 2)
    {
        outColor = vec4(inTexCoords, 0.f, 1.f);
    }
    else if (PushConstants.DrawDebug == 3)
    {
        outColor = vec4(inColor, 1.f);
    }
    // if (outColor.a <= 0.1f)
    //     discard;
}