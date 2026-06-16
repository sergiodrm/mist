
struct Camera
{
	mat4 view;
	mat4 invView;
	mat4 projection;
	mat4 jitteredProjection;
	mat4 invProjection;
	mat4 viewProjection;
	mat4 invViewProjection;
	mat4 jitteredViewProjection;
};

#ifndef _CAMERA_NOT_DEFINE_UNIFORMS_

#ifndef _CAMERA_SET_
#define _CAMERA_SET_ 0
#endif // _CAMERA_SET_

layout (set = _CAMERA_SET_, binding = 0) uniform CameraBlock 
{
	Camera data;
} u_camera;

#ifndef _CAMERA_NOT_PREV_FRAME_
layout (set = _CAMERA_SET_, binding = 1) uniform PrevCameraBlock 
{
	Camera data;
} u_prevCamera;
#endif

#endif
