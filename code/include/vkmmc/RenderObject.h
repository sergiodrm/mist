#pragma once

#include "Mesh.h"

namespace vkmmc
{
	struct RenderObject
	{
		enum : uint32_t { InvalidId = UINT32_MAX };
		uint32_t Id{ InvalidId };

		RenderObject() = default;
		RenderObject(uint32_t id) : Id(id) {}
		inline bool IsValid() const { return Id != InvalidId; }
		inline void Invalidate() { Id = InvalidId; }
		operator uint32_t() const { return Id; }
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
