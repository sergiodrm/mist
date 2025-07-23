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

    // mat4 rotView = mat4(mat3(u_camera.data.invView));
	// vec4 clipPos = u_camera.data.projection * rotView * vec4(inPosition, 1.0f);  
    // gl_Position = clipPos.xyww;
    gl_Position = u_camera.data.invViewProjection * vec4(localPos, 1.f);
}