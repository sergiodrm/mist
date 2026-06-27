#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inLifetime;
layout(location = 2) in vec4 inVelocity;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec4 OutColor;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

#include <shaders/includes/camera.glsl>

void main()
{
    gl_PointSize = 4.f;
    gl_Position = u_camera.data.jitteredViewProjection * vec4(inPosition, 1.f);
    OutColor = inColor;
}