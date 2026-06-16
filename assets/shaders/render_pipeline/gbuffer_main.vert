#version 450

// #extension GL_EXT_nonuniform_qualifier : enable

#include <shaders/includes/model.glsl>
#include <shaders/includes/camera.glsl>
#include <shaders/includes/vertex_mesh.glsl>
#include <shaders/includes/instance_data.glsl>

// Vertex output
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;
layout (location = 5) out vec4 outCurrWSPos;
layout (location = 6) out vec4 outPrevWSPos;
layout (location = 7) out mat3 outTBN;

void main() 
{
	mat4 modelTransform = u_modelBuffer.data[u_instance.data.modelId].worldTransform;
	mat4 prevModelTransform = u_prevModelBuffer.data[u_instance.data.modelId].worldTransform;

	// Compute world space vertex position
	vec4 worldPos = modelTransform * vec4(inPosition,1.f);
	outWorldPos = worldPos.xyz;
	gl_Position = u_camera.data.jitteredViewProjection * worldPos;

	// Compute normals on world space.
	mat3 normalTransform = transpose(inverse(mat3(modelTransform)));

	// Motion vectors with unjittered projections
	outCurrWSPos = u_camera.data.viewProjection * worldPos;
	outPrevWSPos = u_prevCamera.data.viewProjection * prevModelTransform * vec4(inPosition, 1.f);

	// Normals, tangents and TBN
	outNormal = normalize(normalTransform * normalize(inNormal));	
	outTangent = normalize(normalTransform * normalize(inTangent.xyz));
	vec3 B = cross(outNormal, outTangent) * inTangent.w;
	outTBN = mat3(outTangent, B, outNormal);
	
	// Color and UV
	outColor = inColor;
	outUV = inUV0;
}
