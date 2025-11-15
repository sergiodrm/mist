#include "Culling.h"
#include "DebugRender.h"

namespace Mist
{
	CIntVar CVar_EnableCulling("r_enableCulling", 0);

#if 0
	/**
	* https://bruop.github.io/frustum_culling/
	*/
	bool ComputeCulling_BruOp(const glm::mat4& viewProjection, const AABB_t& aabb)
	{
		glm::vec4 corners[] = {
			{aabb.min.x, aabb.min.y, aabb.min.z, 1.0}, // x y z
			{aabb.max.x, aabb.min.y, aabb.min.z, 1.0}, // X y z
			{aabb.min.x, aabb.max.y, aabb.min.z, 1.0}, // x Y z
			{aabb.max.x, aabb.max.y, aabb.min.z, 1.0}, // X Y z

			{aabb.min.x, aabb.min.y, aabb.max.z, 1.0}, // x y Z
			{aabb.max.x, aabb.min.y, aabb.max.z, 1.0}, // X y Z
			{aabb.min.x, aabb.max.y, aabb.max.z, 1.0}, // x Y Z
			{aabb.max.x, aabb.max.y, aabb.max.z, 1.0}, // X Y Z
		};
		bool inside = false;
		for (uint32_t i = 0; i < 8; ++i)
		{
			glm::vec4 c = viewProjection * corners[i];
			inside = inside ||
				c.x >= -c.w && c.x <= c.w
				&& c.y >= -c.w && c.y <= c.w
				&& c.z >= 0.f && c.z <= c.w;
			//if (c.x >= -c.w && c.x <= c.w
			//	&& c.y >= -c.w && c.y <= c.w
			//	&& c.z >= 0.f && c.z <= c.w)
			//	return true;
		}
		return inside;
	}
#endif // 0


	void Frustum::DrawDebug(const glm::vec3& color)
	{
		// near
		DebugRender::DrawLine3D(m_points[0], m_points[1], color);
		DebugRender::DrawLine3D(m_points[0], m_points[2], color);
		DebugRender::DrawLine3D(m_points[1], m_points[3], color);
		DebugRender::DrawLine3D(m_points[2], m_points[3], color);
		// far
		DebugRender::DrawLine3D(m_points[4], m_points[5], color);
		DebugRender::DrawLine3D(m_points[4], m_points[6], color);
		DebugRender::DrawLine3D(m_points[5], m_points[7], color);
		DebugRender::DrawLine3D(m_points[6], m_points[7], color);
		// union
		DebugRender::DrawLine3D(m_points[0], m_points[4], color);
		DebugRender::DrawLine3D(m_points[1], m_points[5], color);
		DebugRender::DrawLine3D(m_points[2], m_points[6], color);
		DebugRender::DrawLine3D(m_points[3], m_points[7], color);
	}
}