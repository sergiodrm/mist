#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec4 inFragPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;

struct LightData
{
    vec4 Pos; // w: radius
    vec4 Color; // w: compression
};

layout (std140, set = 1, binding = 1) uniform Environment
{
    vec4 AmbientColor;
    vec4 ViewPos; // w: num of lights to process
    LightData Lights[8];
    LightData DirectionalLight;
} u_Env;

layout(set = 2, binding = 0) uniform sampler2D texArray[3];

// Debug push constants
layout( push_constant ) uniform constants
{
	int ForcedTexIndex;
	int DrawDebug;
    int EnableLighting;
} PushConstants;

vec3 CalculateLighting(vec3 lightDir, vec3 lightColor)
{
    // Diffuse
    vec3 normal = normalize(inNormal);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.5f;
    vec3 viewDir = normalize(vec3(u_Env.ViewPos) - vec3(inFragPos));
    vec3 reflectDir = reflect(-lightDir, normal);
    // TODO: shininess from material
    float shininess = 32.f;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    return diffuse + specular;
}

vec3 ProcessPointLight(LightData light)
{
    // Attenuation
    float dist = length(vec3(light.Pos) - vec3(inFragPos));
    float r = light.Pos.a;
    float c = light.Color.a;
    float attenuation = pow(smoothstep(r, 0, dist), c);

    vec3 lightDir = normalize(vec3(light.Pos) - vec3(inFragPos));
    vec3 lighting = CalculateLighting(lightDir, vec3(light.Color));
    
    return lighting * attenuation;
}

vec3 ProcessDirectionalLight(LightData light)
{
    vec3 lightDir = normalize(vec3(-light.Pos));
    return CalculateLighting(lightDir, vec3(light.Color));
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
            for (int i = 0; i < int(u_Env.ViewPos.w); ++i)
                lightColor += ProcessPointLight(u_Env.Lights[i]);
            lightColor += ProcessDirectionalLight(u_Env.DirectionalLight);
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
    if (outColor.a <= 0.1f)
        discard;
}