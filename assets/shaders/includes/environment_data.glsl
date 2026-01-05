
#include <shaders/includes/lighting.glsl>

#define MAX_LIGHTS 500

struct Environment
{
    vec3 AmbientColor;
    int NumOfSpotLights;
    vec3 ViewPos;
    int NumOfPointLights;
    LightData Lights[MAX_LIGHTS];
    LightData DirectionalLight;
    LightData SpotLights[MAX_LIGHTS];
};

