
/*
 * Data structures for lighting
*/

struct LightData
{
    vec3 Color;
    float Compression;
    

    vec3 Pos;
    float Radius;
    

    vec3 Dir;
    float _padding;
    

    vec2 CosCutoff; // x: inner, y: outer
    int ShadowMapIndex;
    float Strength;
    
};

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 10
#endif

#ifndef MAX_DIRECTIONAL_LIGHTS
#define MAX_DIRECTIONAL_LIGHTS 1
#endif

#ifndef MAX_SPOT_LIGHTS
#define MAX_SPOT_LIGHTS 10
#endif

struct LightingData
{
    vec3 ambientColor;
    int numOfSpotLights;

    int numOfDirectionalLights;
    int numOfPointLights;
    vec2 _padding;

    LightData pointLights[MAX_POINT_LIGHTS];
    LightData directionalLights[MAX_DIRECTIONAL_LIGHTS];
    LightData spotLights[MAX_SPOT_LIGHTS];
};

/**
 * Input
*/

#ifndef _LIGHTING_DATA_NOT_DEFINE_UNIFORMS_

#ifndef _LIGHTING_DATA_SET_
#error Must define _LIGHTING_DATA_SET_
#endif

layout (std140, set = _LIGHTING_DATA_SET_, binding = 0) uniform LightingDataBlock
{
    LightingData data;
} u_lighting;

#endif
