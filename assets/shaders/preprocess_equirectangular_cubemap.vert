#version 460

#include <shaders/includes/vertex_mesh.glsl>
#include <shaders/includes/camera.glsl>

// output
layout (location = 0) out vec3 localPos;


// uniforms
layout (set = 0, binding = 0) uniform CameraBlock 
{
    Camera data;
} u_camera;


// code
void main() 
{
    localPos = inPosition;
    gl_Position = u_camera.data.viewProjection * vec4(localPos, 1.f);
}