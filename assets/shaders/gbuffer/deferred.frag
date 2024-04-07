#version 460

layout(location = 0) in vec2 inTexCoords;
layout(location = 0) out vec4 outColor;

struct LightData
{
    vec4 Pos; // w: pointlight-radius; directional-project shadows (-1.f not project. >=0.f shadow map index)
    vec4 Color; // w: compression
};

struct SpotLightData
{
    vec4 Color; // w: project shadows (-1.f not project. >=0.f shadow map index)
    vec4 Dir; // w: inner cutoff
    vec4 Pos; // w: outer cutoff
};

// Per frame data
layout (std140, set = 0, binding = 0) uniform Environment
{
    vec4 AmbientColor; // w: num of spot lights to process
    vec4 ViewPos; // w: num of lights to process
    LightData Lights[8];
    LightData DirectionalLight;
    SpotLightData SpotLights[8];
} u_Env;

// GBuffer textures
layout(set = 1, binding = 0) uniform sampler2D u_GBufferPosition;
layout(set = 1, binding = 1) uniform sampler2D u_GBufferNormal;
layout(set = 1, binding = 2) uniform sampler2D u_GBufferAlbedo;

float LinearizeDepth(float z, float n, float f)
{
    return (2.0 * n) / (f + n - z * (f - n));	
}

vec3 CalculateLighting(vec3 fragPos, vec3 fragNormal, vec3 viewPos, vec3 lightDir, vec3 lightColor)
{
    // Diffuse
    vec3 normal = normalize(fragNormal);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.5f;
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    // TODO: shininess from material
    float shininess = 32.f;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    return diffuse + specular;
}

#if 0
vec4 GetLightSpaceCoords(int lightIndex)
{
    if (lightIndex == 0)
        return inLightSpaceFragPos_0;
    if (lightIndex == 1)
        return inLightSpaceFragPos_1;
    if (lightIndex == 2)
        return inLightSpaceFragPos_2;
}


float CalculateShadow(int shadowMapIndex)
{
    vec4 lightSpaceCoord = GetLightSpaceCoords(shadowMapIndex);
    vec4 shadowCoord = lightSpaceCoord / lightSpaceCoord.w;
#if 1
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap[shadowMapIndex], 0);
    float currentDepth = shadowCoord.z;
    float bias = 0.005f;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(u_ShadowMap[shadowMapIndex], shadowCoord.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }    
    }
    shadow /= 9.0;
    return shadow;
#else
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
#endif
}
#endif

vec3 ProcessPointLight(vec3 fragPos, vec3 fragNormal, vec3 viewPos, LightData light)
{
    // Attenuation
    float dist = length(vec3(light.Pos) - fragPos);
    float r = light.Pos.a;
    float c = light.Color.a;
    float attenuation = pow(smoothstep(r, 0, dist), c);

    vec3 lightDir = normalize(vec3(light.Pos) - fragPos);
    vec3 lighting = CalculateLighting(fragPos, fragNormal, viewPos, lightDir, vec3(light.Color));
    return lighting * attenuation;
}

vec3 ProcessDirectionalLight(vec3 fragPos, vec3 fragNormal, vec3 viewPos, LightData light)
{
    vec3 lightDir = normalize(vec3(-light.Pos));
    return CalculateLighting(fragPos, fragNormal, viewPos, lightDir, vec3(light.Color));
}

vec3 ProcessSpotLight(vec3 fragPos, vec3 fragNormal, vec3 viewPos, SpotLightData light)
{
    vec3 lighting = vec3(0.f);
    vec3 lightDir = normalize(vec3(light.Pos) - fragPos);
    float theta = dot(lightDir, -vec3(light.Dir));
    float innerCutoff = light.Dir.a;
    float outerCutoff = light.Pos.a;
    float epsilon = innerCutoff - outerCutoff;
    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    if (intensity > 0.f)
    {
        lighting = CalculateLighting(fragPos, fragNormal, viewPos, lightDir, vec3(light.Color));
    }
    return lighting * intensity;
}

void main()
{
	vec3 fragPos = texture(u_GBufferPosition, inTexCoords).rgb;
	vec3 fragNormal = texture(u_GBufferNormal, inTexCoords).rgb;
	vec4 fragColor = texture(u_GBufferAlbedo, inTexCoords);
	vec3 viewPos = u_Env.ViewPos.xyz;
    outColor = fragColor;
    if (fragColor.a <= 0.1f)
        discard;

    vec3 lightColor = vec3(1.f, 1.f, 1.f);
    
    {
        // Shadows

        // Point lights (no shadows)
        int numPointLights = int(u_Env.ViewPos.w);
        vec3 pointLightsColor = vec3(0.f);
        for (int i = 0; i < numPointLights; ++i)
            pointLightsColor += ProcessPointLight(fragPos, fragNormal, viewPos, u_Env.Lights[i]);

        // Spot lights
        int numSpotLights = int(u_Env.AmbientColor.w);
        vec3 spotLightsColor = vec3(0.f);
        float spotLightShadow = 0.f;
        for (int i = 0; i < numSpotLights; ++i)
        {
            vec3 spotLightColor = ProcessSpotLight(fragPos, fragNormal, viewPos, u_Env.SpotLights[i]);
#if 0
            if (u_Env.SpotLights[i].Color.w >= 0.f)
            {
                int shadowIndex = int(u_Env.SpotLights[i].Color.w);
                float shadow = CalculateShadow(shadowIndex);
                spotLightColor *= (1.f-shadow);
            }
#endif
            spotLightsColor += spotLightColor;
        }
        spotLightsColor *= (1.f-spotLightShadow);
        // Directional light
        vec3 directionalLightColor = ProcessDirectionalLight(fragPos, fragNormal, viewPos, u_Env.DirectionalLight);
#if 0
        if (u_Env.DirectionalLight.Pos.w >= 0.f)
        {
            int shadowIndex = int(u_Env.DirectionalLight.Pos.w);
            float shadow = CalculateShadow(shadowIndex);
            directionalLightColor *= (1.f - shadow);
        }
#endif

        lightColor = (pointLightsColor + spotLightsColor + directionalLightColor);
        // Mix
        lightColor = vec3(u_Env.AmbientColor) + lightColor;
        //lightColor = vec3(shadow);
        //lightColor = vec3(shadowCoord);
    }
    outColor = vec4(lightColor, 1.f) * outColor;
}