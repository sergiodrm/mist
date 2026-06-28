
/**
 * PBR functions
*/

#define F0_Default vec3(0.04f)

// The Fresnel-Schlick approximation expects a F0 parameter 
// which is known as the surface reflection at zero incidence 
// or how much the surface reflects if looking directly at the surface.
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.f-F0) * pow(clamp(1.f-cosTheta, 0.f, 1.f), 5.f);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.f);
    float NdotH2 = NdotH*NdotH;

    float num = a2;
    float denom = NdotH2 * (a2 - 1.f) + 1.f;
    denom = M_PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.f;
    float k = (r*r)/8.f;
    float num = NdotV;
    float denom = NdotV * (1.f-k) + k;
    return num/denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.f);
    float NdotL = max(dot(N, L), 0.f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1*ggx2;
}

vec3 CalculateBRDF(vec3 normal, vec3 lightDir, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 viewDir, vec3 halfway)
{
    vec3 F0 = mix(F0_Default, albedo, metallic);
    vec3 F = FresnelSchlick(max(dot(halfway, viewDir), 0.f), F0);

    // Normal distribution
    float NDF = DistributionGGX(normal, halfway, roughness);
    // Geometry distribution
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    // Calculate Cook-Torrance BRDF
    vec3 num = NDF * G * F;
    float denom = 4.f * max(dot(normal, viewDir), 0.f) * max(dot(normal, lightDir), 0.f) + 0.0001f;
    vec3 specular = num/denom;
#ifdef PBR_DEBUG_SPECULAR
    return specular;
#endif // PBRDEBUG_SPECULAR

    // Light contribution
    vec3 kS = F; // specular contribution
    vec3 kD = vec3(1.f)-kS; // diffuse contribution (energy conservation)
    kD *= 1.f-metallic;

    float NdotL = max(dot(normal, lightDir), 0.f);
    return (kD * albedo / M_PI + specular) * radiance * NdotL;
}
