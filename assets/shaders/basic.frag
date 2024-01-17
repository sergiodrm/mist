#version 460

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;

layout(set = 1, binding = 0) uniform sampler2D texArray[3];

// Debug push constants
layout( push_constant ) uniform constants
{
	int ForcedTexIndex;
	int DrawDebug;
} PushConstants;

void main()
{
    if (PushConstants.ForcedTexIndex > -1)
    {
        outColor = texture(texArray[PushConstants.ForcedTexIndex], inTexCoords);
    }
    else
    {
        outColor = texture(texArray[0], inTexCoords);
    }
    if (PushConstants.DrawDebug == 1)
    {
        outColor = vec4(inNormal, 1.f);
    } 
    else if (PushConstants.DrawDebug == 2)
    {
        outColor = vec4(inTexCoords, 0.f, 1.f);
    }
    else if (PushConstants.DrawDebug == 3)
    {
        outColor = vec4(inColor, 1.f);
    }
    // if (outColor.a <= 0.1f)
    //     discard;
}