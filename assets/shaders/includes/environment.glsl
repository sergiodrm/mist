
#include <shaders/includes/lighting.glsl>

// Per frame data
struct Environment
{
    vec4 AmbientColor;  // w: num of spot lights to process
    vec4 ViewPos;       // w: num of lights to process
    LightData Lights[8];
    LightData DirectionalLight;
    SpotLightData SpotLights[8];
};
