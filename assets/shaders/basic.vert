#version 460

layout (location = 0) in vec3 LSPosition;

void main()
{
    gl_Position = vec4(LSPosition, 1.f);
}