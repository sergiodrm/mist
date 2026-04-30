#version 460

#include <shaders/includes/camera.glsl>

// output
layout (location = 0) out vec3 localPos;


// uniforms
layout (set = 0, binding = 0) uniform CameraBlock 
{
    Camera data;
} u_camera;

#define _VIEW_PROJ_MATRIX u_camera.data.viewProjection
#include <shaders/includes/vertex_mesh.glsl>

// code
void main() 
{
    localPos = inPosition;
    gl_Position = Vertex_ComputeWorldPosToClipSpace(vec4(localPos, 1.f));
}