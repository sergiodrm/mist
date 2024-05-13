#version 450


layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

layout(set = 2, binding = 0) uniform sampler2D u_Textures[3];

void main() 
{
	outPosition = vec4(inWorldPos, 1.0);

	// Calculate normal in tangent space
	vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent);
	vec3 B = cross(N, T);
	mat3 TBN = mat3(T, B, N);
	vec3 tnorm = TBN * normalize(texture(u_Textures[1], inUV).xyz * 2.0 - vec3(1.0));
	outNormal = normalize(vec4(inNormal, 1.0));
	//outNormal = vec4(inTangent, 1.0);

	outAlbedo = texture(u_Textures[0], inUV);
	if (outAlbedo.a <= 0.1)
		discard;
}