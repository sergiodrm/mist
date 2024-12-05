#pragma once

#include <glm/glm.hpp>

namespace Mist
{
	class ShaderProgram;
	class cTexture;
	struct RenderContext;

	namespace DebugRender
	{
		void Init(const RenderContext& context);
		void Destroy(const RenderContext& context);

		void DrawLine3D(const glm::vec3& init, const glm::vec3& end, const glm::vec3& color);
		void DrawAxis(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl);
		void DrawAxis(const glm::mat4& transform);
		void DrawSphere(const glm::vec3& pos, float radius, const glm::vec3& color, uint32_t vertices = 16);
		void DrawScreenQuad(const glm::vec2& screenPos, const glm::vec2& size, const cTexture& texture, uint32_t view = 0);

		void Draw(const RenderContext& context);
	}	
}