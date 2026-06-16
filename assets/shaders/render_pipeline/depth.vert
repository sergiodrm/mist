#version 460


layout (std140, set = 0, binding = 0) uniform UBO
{
    mat4 depthVP;
} u_ubo;


#define _MODEL_NOT_PREV_FRAME_
#include <shaders/includes/model.glsl>
#include <shaders/includes/instance_data.glsl>
#include <shaders/includes/vertex_mesh.glsl>

void main()
{
    gl_Position = u_ubo.depthVP * u_modelBuffer.data[u_instance.data.modelId].worldTransform * vec4(inPosition, 1.f);
}