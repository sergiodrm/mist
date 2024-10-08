#version 450

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec2 inUV;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 View;
	mat4 Projection;
	mat4 ViewProjection;
} u_camera;
layout (set = 1, binding = 0) uniform ModelData
{
	mat4 Model;
} u_model;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;

void main() 
{
	// Vertex position in world space
	vec3 worldPos = vec3(u_model.Model * inPos);
	gl_Position = u_camera.ViewProjection * vec4(worldPos, 1.f);
#define VIEW_SPACE_TRANSFORMS
#ifdef VIEW_SPACE_TRANSFORMS
	mat3 normalTransform = transpose(inverse(mat3(u_camera.View * u_model.Model)));
	outWorldPos = vec3(u_camera.View * vec4(worldPos, 1.f));
#else
	mat3 normalTransform = transpose(inverse(mat3(u_model.Model)));
	outWorldPos = worldPos;
#endif

	
	// Normal in world space
	outNormal = normalTransform * normalize(inNormal);	
	outTangent = normalTransform * normalize(inTangent);
	
	// Currently just vertex color
	outColor = inColor;
	// uvs
	outUV = inUV;
}
