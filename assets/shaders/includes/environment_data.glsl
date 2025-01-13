
#include <shaders/includes/lighting.glsl>

struct Environment
{
    vec3 AmbientColor;
    int NumOfSpotLights;
    vec3 ViewPos;
    int NumOfPointLights;
    LightData Lights[8];
    LightData DirectionalLight;
    LightData SpotLights[8];
};

