#version 460

layout(location = 0) in vec2 InPosition;
layout(location = 1) in vec2 InVelocity;
layout(location = 2) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

void main()
{
    gl_PointSize = 3.f;
    gl_Position = vec4(InPosition.xy, 0.f, 1.f);
    OutColor = InColor;
}
