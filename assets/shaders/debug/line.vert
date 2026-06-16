#version 460

#define _CAMERA_NOT_PREV_FRAME_
#include <shaders/includes/camera.glsl>

layout (location = 0) in vec4 a_Position;
layout (location = 1) in vec4 a_Color;

layout (location = 0) out vec4 vs_Color;

void main()
{
    gl_Position = u_camera.data.viewProjection * a_Position;
    vs_Color = a_Color;
}