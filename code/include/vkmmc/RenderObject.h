#pragma once

#include "Mesh.h"

namespace vkmmc
{
	class RenderObject
	{
	public:

		glm::mat4 GetTransform() const;

		Mesh m_mesh;
		Material m_material;

		glm::vec3 m_position{ 0.f };
		glm::vec3 m_rotation{ 0.f };
		glm::vec3 m_scale{ 1.f };
	};
}
