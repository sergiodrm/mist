#version 460


// Color output
layout(location = 0) out vec4 outColor;

// Vertex shader input
layout(location = 0) in vec4 inFragPosWS;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;
//layout(location = 4) in vec4 inLightSpaceFragPos_0;
//layout(location = 5) in vec4 inLightSpaceFragPos_1;
//layout(location = 6) in vec4 inLightSpaceFragPos_2;
layout(location = 4) in vec3 inViewDir;
layout(location = 5) in mat3 inTBN;

//#include <shaders/includes/material.glsl>
//#include <shaders/includes/instance_data.glsl>
//#include <shaders/includes/lighting.glsl>


// vec3 ComputeNormalMapping(vec3 normal)
// {
//     if (MATERIAL_HAS_NORMAL_MAP(u_materialBuffer.data[u_instance.data.materialId].Flags.x))
//     {
//         vec3 normalValue = texture(u_materialTextures[MATERIAL_TEXTURE_NORMAL], inTexCoords).rgb;
//         normalValue = normalize(normalValue*2.f - vec3(1.f));
//         return normalize(inTBN * normalValue);
//     }
//     else
//     {
//         return normalize(normal);
//     }
// }

// vec3 ComputeCubemapReflection(vec3 fragPos, vec3 normal, vec3 viewPos)
// {
//     vec3 I = normalize(fragPos - viewPos);
//     vec3 R = reflect(I, normalize(normal));
//     return texture(u_cubemap, R).rgb;
// }

// vec4 DoLighting(vec3 fragPos, vec3 viewDir, vec3 normal, vec3 albedo, float opacity, float metallic, float roughness)
// {
//     ShadowInfo shadowInfo;
//     shadowInfo.ShadowCoordArray[0] = inLightSpaceFragPos_0;
//     shadowInfo.ShadowCoordArray[1] = inLightSpaceFragPos_1;
//     shadowInfo.ShadowCoordArray[2] = inLightSpaceFragPos_2;
    
//     vec3 lightColor = DoEnvironmentLighting(fragPos, viewDir, normal, albedo, metallic, roughness, 1.f, shadowInfo);
//     vec4 color = vec4(lightColor, opacity);

// //#ifdef CUBEMAP_REFLECTION
// //    // Cubemap reflection
// //    vec3 reflection = ComputeCubemapReflection(fragPos, normal, vec3(0.f));
// //    color = vec4(mix(color.rgb, reflection, 0.02f*metallic), color.a);
// //#endif // CUBEMAP_REFLECTION
// //#ifdef DEBUG_SPECULAR
// //    color = vec4(lightColor, 1.f);
// //#endif // DEBUG_SPECULAR
//     return color;
// }

void main()
{
    outColor = vec4(1,0,0,0.2);
    //MaterialPBR mtl = Material_ReadPBR(u_materialBuffer.data[u_instance.data.materialId], inTexCoords, inNormal);
    
    //vec4 color = vec4(mtl.albedo,1.f);//DoLighting(inFragPosWS.xyz, inViewDir, mtl.normal, mtl.albedo, mtl.opacity, mtl.metallic, mtl.roughness);
    //outColor = color;
}
