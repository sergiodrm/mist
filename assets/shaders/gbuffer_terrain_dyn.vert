#version 450

#include <shaders/includes/model.glsl>
#include <shaders/includes/camera.glsl>


// Vertex output
layout (location = 0) out vec2 outTexCoords;
layout (location = 1) out vec3 outWorldPos;
layout (location = 2) out vec4 outCurrWSPos;
layout (location = 3) out vec4 outPrevWSPos;


// Uniforms
layout (set = 0, binding = 0) uniform CameraBlock 
{
	Camera data;
} u_camera;

layout (set = 0, binding = 1) uniform PrevCameraBlock 
{
	Camera data;
} u_prevCamera;

layout (set = 4, binding = 0) uniform ModelBlock
{
	Model data;
} u_model;

#define VERTEX_MESH_DISABLE_DEFAULT_VERTEX
#include <shaders/includes/vertex_mesh.glsl>
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoords;

void main() 
{
	// Compute world space vertex position
	vec4 worldPos = u_model.data.worldTransform * vec4(inPosition,1.f);
	gl_Position = worldPos;

	// motion vectors
	outCurrWSPos = u_camera.data.viewProjection * worldPos;
	outPrevWSPos = u_prevCamera.data.viewProjection * worldPos;

	outWorldPos = worldPos.xyz;
	outTexCoords = inTexCoords;
}
