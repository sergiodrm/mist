#pragma once

#include <glm/glm.hpp>

namespace Mist
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Color;
		glm::vec3 Tangent;
		glm::vec2 TexCoords;
	};
	
}
