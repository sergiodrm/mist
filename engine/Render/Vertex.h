#pragma once

#include <glm/glm.hpp>

namespace Mist
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Color;
		glm::vec4 Tangent;
		glm::vec2 TexCoords0;
		glm::vec2 TexCoords1;
	};
	
}
