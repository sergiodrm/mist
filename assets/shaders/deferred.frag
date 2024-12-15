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
    // x: radius, y: compression, zw: unused (padding)
    vec4 Params;
};

#define MAX_SHADOW_MAPS 3

// Per frame data
layout (std140, set = 0, binding = 0) uniform Environment
{
    vec4 AmbientColor; // w: num of spot lights to process
    vec4 ViewPos; // w: num of lights to process
    LightData Lights[8];
    LightData DirectionalLight;
    SpotLightData SpotLights[8];
} u_Env;
layout(set = 0, binding = 1) uniform ShadowMapInfo
{
    mat4 LightViewMat[MAX_SHADOW_MAPS];
} u_ShadowMapInfo;

// GBuffer textures
layout(set = 1, binding = 0) uniform sampler2D u_GBufferPosition;
layout(set = 2, binding = 0) uniform sampler2D u_GBufferNormal;
layout(set = 3, binding = 0) uniform sampler2D u_GBufferAlbedo;
// SSAO texture
layout(set = 4, binding = 0) uniform sampler2D u_ssao;
// Shadow map textures
layout(set = 5, binding = 0) uniform sampler2D u_ShadowMap[MAX_SHADOW_MAPS];

#define LightRadius(l) l.Pos.a
#define LightCompression(l) l.Color.w
#define M_PI 3.1415926535897932384626433832795

float LinearizeDepth(float z, float n, float f)
{
    return (2.0 * n) / (f + n - z * (f - n));	
}

float CalculateAttenuation(float distance, float radius, float compression)
{
#if 1
    return pow(smoothstep(radius, 0, distance), compression);
#else
    return 1.f / (distance * distance);
#endif
}

// The Fresnel-Schlick approximation expects a F0 parameter 
// which is known as the surface reflection at zero incidence 
// or how much the surface reflects if looking directly at the surface.
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.f-F0) * pow(clamp(1.f-cosTheta, 0.f, 1.f), 5.f);
}

float DistributionGGX(vec3 N, vec3 H, float Roughness)
{
    float a = Roughness*Roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.f);
    float NdotH2 = NdotH*NdotH;

    float num = a2;
    float denom = NdotH2 * (a2 - 1.f) + 1.f;
    denom = M_PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float Roughness)
{
    float r = Roughness + 1.f;
    float k = (r*r)/8.f;
    float num = NdotV;
    float denom = NdotV * (1.f-k) + k;
    return num/denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float Roughness)
{
    float NdotV = max(dot(N, V), 0.f);
    float NdotL = max(dot(N, L), 0.f);
    float ggx2 = GeometrySchlickGGX(NdotV, Roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, Roughness);
    return ggx1*ggx2;
}

vec3 CalculateBRDF(vec3 Normal, vec3 LightDir, vec3 Radiance, vec3 Albedo, float Metallic, float Roughness, vec3 ViewDir, vec3 Halfway)
{
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Albedo, Metallic);
    vec3 F = FresnelSchlick(max(dot(Halfway, ViewDir), 0.f), F0);

    // Normal distribution
    float NDF = DistributionGGX(Normal, Halfway, Roughness);
    // Geometry distribution
    float G = GeometrySmith(Normal, ViewDir, LightDir, Roughness);
    // Calculate Cook-Torrance BRDF
    vec3 Num = NDF * G * F;
    float Denom = 4.f * max(dot(Normal, ViewDir), 0.f) * max(dot(Normal, LightDir), 0.f) + 0.0001f;
    vec3 Specular = Num/Denom;

    // Light contribution
    vec3 kS = F; // specular contribution
    vec3 kD = vec3(1.f)-kS; // diffuse contribution (energy conservation)
    kD *= 1.f-Metallic;

    float NdotL = max(dot(Normal, LightDir), 0.f);
    return (kD * Albedo / M_PI + Specular) * Radiance * NdotL;
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

#if 1
vec4 GetLightSpaceCoords(vec4 fragPos, int lightIndex)
{
    return u_ShadowMapInfo.LightViewMat[lightIndex] * fragPos;
}

float CalculateShadow(vec4 fragPos, int shadowMapIndex)
{
    vec4 lightSpaceCoord = GetLightSpaceCoords(fragPos, shadowMapIndex);
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

vec3 ProcessPointLight(vec3 fragPos, vec3 fragNormal, LightData Light, vec3 Albedo, float Metallic, float Roughness)
{
    // Radiance calculation with attenuation
    vec3 LightPos = Light.Pos.xyz;
    vec3 LightDir = LightPos - fragPos;
    vec3 V = normalize(-fragPos);
    vec3 L = normalize(LightDir);
    vec3 H = normalize(V + L);
    float Distance = length(LightDir);
    float Radius = LightRadius(Light);
    float Compression = LightCompression(Light);
    float Attenuation = CalculateAttenuation(Distance, Radius, Compression);
    vec3 Radiance = Light.Color.rgb * Attenuation;

    return CalculateBRDF(fragNormal, L, Radiance, Albedo, Metallic, Roughness, V, H);
}

vec3 ProcessDirectionalLight(vec3 fragPos, vec3 fragNormal, LightData light, vec3 Albedo, float Metallic, float Roughness)
{
    vec3 V = normalize(-fragPos);
    vec3 lightDir = normalize(vec3(-light.Pos));
    vec3 H = normalize(V + lightDir);
    vec3 Radiance = light.Color.rgb;
    return CalculateBRDF(fragNormal, lightDir, Radiance, Albedo, Metallic, Roughness, V, H);
    //return CalculateLighting(fragPos, fragNormal, viewPos, lightDir, vec3(light.Color));
}

vec3 ProcessSpotLight(vec3 fragPos, vec3 fragNormal, SpotLightData light, vec3 Albedo, float Metallic, float Roughness)
{
    vec3 lighting = vec3(0.f);
    vec3 lightDir = normalize(vec3(light.Pos) - fragPos);
    float theta = dot(lightDir, -vec3(light.Dir));
    float cosCutoff = light.Dir.a;
    float cosOuterCutoff = light.Pos.a;
    float epsilon = max(cosCutoff - cosOuterCutoff, 0.f);
    float intensity = clamp((theta - cosOuterCutoff) / epsilon, 0.0, 1.0);
    if (intensity > 0.f)
    {
        float radius = light.Params.x;
        float compression = light.Params.y;
        float distance = length(light.Pos.xyz - fragPos);
        float attenuation = CalculateAttenuation(distance, radius, compression);
        vec3 Radiance = light.Color.rgb * attenuation;
        vec3 V = normalize(-fragPos);
        vec3 H = normalize(V + lightDir);
        lighting = CalculateBRDF(fragNormal, lightDir, Radiance, Albedo, Metallic, Roughness, V, H);
        //lighting = CalculateLighting(fragPos, fragNormal, viewPos, lightDir, vec3(light.Color));
    }
    //intensity *= 10.f;
    return lighting * intensity;
}


vec4 main_PBR(vec3 FragViewPos, vec3 Normal, vec3 Albedo, float Metallic, float Roughness, float AO)
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(-FragViewPos);
    vec3 Lo = vec3(0.f);

    // Point lights
    int numPointLights = int(u_Env.ViewPos.w);
    for (int i = 0; i < numPointLights; ++i)
    {
        LightData Light = u_Env.Lights[i];
        Lo += ProcessPointLight(FragViewPos, Normal, Light, Albedo, Metallic, Roughness);
    }

    // Spot lights
    int numSpotLights = int(u_Env.AmbientColor.w);
    vec3 spotLightColor = vec3(0.f);
    float spotLightShadow = 0.f;
    for (int i = 0; i < numSpotLights; ++i)
    {
        SpotLightData Light = u_Env.SpotLights[i];
        spotLightColor += ProcessSpotLight(FragViewPos, N, Light, Albedo, Metallic, Roughness);
#if 1
        if (u_Env.SpotLights[i].Color.w >= 0.f)
        {
            int shadowIndex = int(u_Env.SpotLights[i].Color.w);
            float shadow = CalculateShadow(vec4(FragViewPos, 1.f), shadowIndex);
            //shadow = 1.f - shadow;
            spotLightColor *= (1.f-shadow);
            //return vec4(shadow, shadow, shadow, 1.f);
        }
#endif
    }
    spotLightColor *= (1.f-spotLightShadow);
    Lo += spotLightColor;
    //return vec4(spotLightColor, 1.f);

    // Directional light
    vec3 directionalLightColor = ProcessDirectionalLight(FragViewPos, N, u_Env.DirectionalLight, Albedo, Metallic, Roughness);
    if (u_Env.DirectionalLight.Pos.w >= 0.f)
    {
        int shadowIndex = int(u_Env.DirectionalLight.Pos.w);
        float shadow = CalculateShadow(vec4(FragViewPos, 1.f), shadowIndex);
        directionalLightColor *= (1.f - shadow);
    }
    Lo += directionalLightColor;

    vec3 Ambient = vec3(u_Env.AmbientColor) * Albedo * AO;
    vec3 Color = Ambient + Lo;
    //Color = Color / (Color + vec3(1.f));
    //Color = pow(Color, vec3(1.f/2.2f));

    return vec4(Color, 1.f);
}

void main()
{
#if 1
    vec4 gbufferPos = texture(u_GBufferPosition, inTexCoords);
    vec4 gbufferNormal = texture(u_GBufferNormal, inTexCoords);
	vec3 fragPos = gbufferPos.rgb;
	vec3 fragNormal = normalize(gbufferNormal.xyz);
	vec4 fragColor = texture(u_GBufferAlbedo, inTexCoords);
    float metallic = gbufferPos.a;
    float roughness = gbufferNormal.a;
    float ao = texture(u_ssao, inTexCoords).r;
    if (fragColor.a <= 0.1f)
        discard;
    outColor = main_PBR(fragPos, fragNormal, fragColor.rgb, metallic, roughness, ao);
    return;


#else // non pbr

	vec3 fragPos = texture(u_GBufferPosition, inTexCoords).rgb;
	vec3 fragNormal = normalize(texture(u_GBufferNormal, inTexCoords).rgb);
	vec4 fragColor = texture(u_GBufferAlbedo, inTexCoords);
	vec3 viewPos = vec3(0.f);//u_Env.ViewPos.xyz;
    outColor = fragColor;
    //outColor = vec4(fragPos, 1.0);
    //return;
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
                float shadow = CalculateShadow(vec4(fragPos, 1.f), shadowIndex);
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
        vec3 ambientColor = vec3(u_Env.AmbientColor) * texture(u_ssao, inTexCoords).r;
        lightColor = ambientColor + lightColor;
        //lightColor = vec3(shadow);
        //lightColor = vec3(shadowCoord);
    }
    outColor = vec4(lightColor, 1.f) * outColor;
#endif
}