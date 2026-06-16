#version 460

#define _CAMERA_NOT_PREV_FRAME_
#include <shaders/includes/camera.glsl>
#include <shaders/includes/vertex_mesh.glsl>

// output
layout (location = 0) out vec3 localPos;


// code
void main() 
{
    localPos = inPosition;
    gl_Position = u_camera.data.viewProjection * vec4(localPos, 1.f);
}