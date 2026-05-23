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

#define VERTEX_MESH_DISABLE_DEFAULT_VERTEX
#include <shaders/includes/vertex_mesh.glsl>
layout (location = 0) in vec3 inPosition;

void main() 
{
	// Compute world space vertex position
	vec3 worldPos = inPosition;
	gl_Position = u_camera.data.viewProjection * vec4(worldPos, 1.f);

	// motion vectors
	outCurrWSPos = u_camera.data.viewProjection * vec4(inPosition,1.f);
	outPrevWSPos = u_prevCamera.data.viewProjection * vec4(inPosition, 1.f);

	outWorldPos = worldPos;
	outTexCoords = vec2(1,0);
}
