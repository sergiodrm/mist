#version 450

#include <shaders/includes/model.glsl>
#include <shaders/includes/camera.glsl>

// Vertex input
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inUV;

// Vertex output
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;
layout (location = 5) out mat3 outTBN;

// Uniforms
layout (set = 0, binding = 0) uniform CameraBlock 
{
	Camera data;
} u_camera;

layout (set = 1, binding = 0) uniform ModelBlock
{
	Model data;
} u_model;


void main() 
{
	// Vertex position in world space
	vec3 worldPos = vec3(u_model.data.worldTransform * vec4(inPos,1.f));
	//vec3 worldPos = vec3(u_model.Model * inPos);
	gl_Position = u_camera.data.invViewProjection * vec4(worldPos, 1.f);
#define VIEW_SPACE_TRANSFORMS
#ifdef VIEW_SPACE_TRANSFORMS
	//mat3 normalTransform = transpose(inverse(mat3(u_camera.View * u_model.Model)));
	mat3 normalTransform = mat3(u_camera.data.invView * u_model.data.worldTransform);
	outWorldPos = vec3(u_camera.data.invView * vec4(worldPos, 1.f));
#else
	mat3 normalTransform = transpose(inverse(mat3(u_model.data.worldTransform)));
	outWorldPos = worldPos;
#endif

	
	// Normal in world space
	outNormal = normalize(normalTransform * normalize(inNormal));	
	outTangent = normalize(normalTransform * normalize(inTangent));
	vec3 B = cross(outNormal, outTangent);
	outTBN = mat3(outTangent, B, outNormal);
	
	// Currently just vertex color
	outColor = inColor;
	// uvs
	outUV = inUV;
}
