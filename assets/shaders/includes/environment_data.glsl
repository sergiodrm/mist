
#include <shaders/includes/lighting.glsl>

#define MAX_LIGHTS 500

struct Environment
{
    vec3 ambientColor;
    int numOfSpotLights;

    int numOfDirectionalLights;
    int numOfPointLights;
    vec2 _padding;

    LightData pointLights[MAX_LIGHTS];
    LightData directionalLights[5];
    LightData spotLights[MAX_LIGHTS];
};

