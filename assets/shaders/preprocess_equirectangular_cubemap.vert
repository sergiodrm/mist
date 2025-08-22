#version 460

//layout (location = 0) in vec3 a_Pos;

#include <shaders/includes/vertex_mesh.glsl>

layout (location = 0) out vec3 localPos;

#include <shaders/includes/camera.glsl>

layout (set = 0, binding = 0) uniform CameraBlock 
{
    Camera data;
} u_camera;

void main() 
{
    localPos = inPosition;
    gl_Position = u_camera.data.viewProjection * vec4(localPos, 1.f);
}