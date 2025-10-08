#version 450

#include <shaders/includes/model.glsl>
#include <shaders/includes/camera.glsl>

#include <shaders/includes/vertex_mesh.glsl>

// Vertex input
//layout (location = 0) in vec3 inPosition;
//layout (location = 1) in vec3 inNormal;
//layout (location = 2) in vec3 inColor;
//layout (location = 3) in vec3 inTangent;
//layout (location = 4) in vec2 inUV;

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
	// Compute world space vertex position
	vec3 worldPos = vec3(u_model.data.worldTransform * vec4(inPosition,1.f));
	
	gl_Position = u_camera.data.viewProjection * vec4(worldPos, 1.f);

	// Compute normals on view space.
	//mat3 normalTransform = mat3(u_camera.data.view * u_model.data.worldTransform);
	mat3 normalTransform = transpose(inverse(mat3(u_camera.data.view * u_model.data.worldTransform)));

	outWorldPos = vec3(u_camera.data.view * vec4(worldPos, 1.f));
	outNormal = normalize(normalTransform * normalize(inNormal));	
	outTangent = normalize(normalTransform * normalize(inTangent.xyz));
	vec3 B = cross(outNormal, outTangent) * inTangent.w;
	outTBN = mat3(outTangent, B, outNormal);
	
	outColor = inColor;
	outUV = inUV0;
}
