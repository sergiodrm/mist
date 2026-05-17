#pragma once

#include <glm/glm.hpp>

namespace Mist
{
	enum VertexAttributeBit
	{
		VertexAttribute_None = 0,
		VertexAttribute_Position = 1<<0,
		VertexAttribute_Normal = 1<<1,
		VertexAttribute_Color = 1<<2,
		VertexAttribute_Tangent = 1<<3,
		VertexAttribute_TexCoord0 = 1<<4,
		VertexAttribute_TexCoord1 = 1<<5,

		VertexAttribute_All = 0xffffffff,
	};
	using VertexAttributeMask = uint32_t;

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
