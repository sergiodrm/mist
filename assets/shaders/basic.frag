#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec4 inFragPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;
layout(location = 4) in vec4 inLightSpaceFragPos;

struct LightData
{
    vec4 Pos; // w: radius
    vec4 Color; // w: compression
};

struct SpotLightData
{
    vec4 Color;
    vec4 Dir; // w: inner cutoff
    vec4 Pos; // w: outer cutoff
};

// Per frame data
layout (std140, set = 0, binding = 2) uniform Environment
{
    vec4 AmbientColor; // w: num of spot lights to process
    vec4 ViewPos; // w: num of lights to process
    LightData Lights[8];
    LightData DirectionalLight;
    SpotLightData SpotLights[8];
} u_Env;

layout (set = 0, binding = 3) uniform sampler2D u_ShadowMap[2];

// Per draw data
layout(std140, set = 1, binding = 1) uniform Material 
{
    float Shininess;
} u_Material;
layout(set = 2, binding = 0) uniform sampler2D u_Textures[3];

float LinearizeDepth(float z, float n, float f)
{
    return (2.0 * n) / (f + n - z * (f - n));	
}

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

float CalculateShadow(vec4 shadowCoord)
{
#if 0
    float shadow = 0.3f;
    float z = shadowCoord.z;
	if ( z > -1.0 && z < 1.0 ) 
	{
		float dist = texture( u_ShadowMap[0], shadowCoord.xy ).r;
        const float bias = 0.005f;
		if ( shadowCoord.w > 0.0 && z - bias > dist ) 
		{
			shadow = 1.f;
		}
	}
	return shadow;
#else
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap[0], 0);
    float currentDepth = shadowCoord.z;
    float bias = 0.005f;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(u_ShadowMap[0], shadowCoord.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
#endif
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

vec3 ProcessSpotLight(SpotLightData light)
{
    vec3 lighting = vec3(0.f);
    vec3 lightDir = normalize(vec3(light.Pos) - vec3(inFragPos));
    float theta = dot(lightDir, -vec3(light.Dir));
    float innerCutoff = light.Dir.a;
    float outerCutoff = light.Pos.a;
    float epsilon = innerCutoff - outerCutoff;
    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    if (intensity > 0.f)
    {
        lighting = CalculateLighting(lightDir, vec3(light.Color));
    }
    return lighting * intensity;
}

void main()
{
    outColor = texture(u_Textures[0], inTexCoords);
    if (outColor.a <= 0.1f)
        discard;
    vec3 lightColor = vec3(1.f, 1.f, 1.f);
    //if (PushConstants.EnableLighting != 0)
    {

        // Shadows
        vec4 shadowCoord = inLightSpaceFragPos / inLightSpaceFragPos.w;
        float shadow = CalculateShadow(shadowCoord);
        // Point lights
        int numPointLights = int(u_Env.ViewPos.w);
        vec3 pointLightsColor = vec3(0.f);
        for (int i = 0; i < numPointLights; ++i)
            pointLightsColor += ProcessPointLight(u_Env.Lights[i]);
        // Spot lights
        int numSpotLights = int(u_Env.AmbientColor.w);
        vec3 spotLightsColor = vec3(0.f);
        for (int i = 0; i < numSpotLights; ++i)
            spotLightsColor += ProcessSpotLight(u_Env.SpotLights[i]);
        spotLightsColor *= (1.f-shadow);
        // Directional light
        vec3 directionalLightColor = ProcessDirectionalLight(u_Env.DirectionalLight);

        lightColor = pointLightsColor + spotLightsColor + directionalLightColor;
        // Mix
        lightColor = vec3(u_Env.AmbientColor) + lightColor;
        //lightColor = vec3(shadow);
        //lightColor = vec3(shadowCoord);
    }
    outColor = vec4(lightColor, 1.f) * outColor;
}