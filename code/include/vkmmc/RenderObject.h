#pragma once

#include "Mesh.h"

namespace vkmmc
{
	struct RenderObject
	{
		uint32_t Id{ UINT32_MAX };
	};

	struct RenderObjectTransform
	{
		glm::vec3 Position{ 0.f };
		glm::vec3 Rotation{ 0.f };
		glm::vec3 Scale{ 1.f };
	};

	glm::mat4 CalculateTransform(const RenderObjectTransform& objectTransform);

	struct RenderObjectMesh
	{
		Mesh StaticMesh;
		Material Mtl;
	};

}
