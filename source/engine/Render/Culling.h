#pragma once

#include "Utils/GenericUtils.h"
#include "Core/Logger.h"
#include "Application/CmdParser.h"

namespace Mist
{
	extern CIntVar CVar_EnableCulling;

	struct AABB_t
	{
		glm::vec3 min;
		glm::vec3 max;

		glm::vec3 GetCenter() const { return (max + min) * 0.5f; }
		glm::vec3 GetExtent() const { return { max.x - min.x, max.y - min.y, max.z - min.z }; }

		static const AABB_t& InvalidAABB() 
		{
			static AABB_t invalidAABB = { glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) };
			return invalidAABB;
		}
		void Invalidate() { *this = InvalidAABB(); }
		void BuildFromCenterAndExtent(const glm::vec3& center, const glm::vec3& extent);
		AABB_t ApplyTransform(const glm::mat4& transform) const;
	};

	//https://gist.github.com/podgorskiy/e698d18879588ada9014768e3e82a644
	class Frustum
	{
		enum PlaneSide
		{
			Left,
			Right,
			Bottom,
			Top,
			Near,
			Far,
			Count,
			Combinations = Count * (Count - 1) / 2
		};
	public:
		Frustum() = default;
		Frustum(const glm::mat4& viewProjectionMatrix);

		bool IsBoxVisible(const glm::vec3& minp, const glm::vec3& maxp) const;

		void DrawDebug(const glm::vec3& color = glm::vec3(1,0,0));

	private:
		template<PlaneSide i, PlaneSide j>
		struct ij2k { enum { k = i * (9 - i) / 2 + j - 1 }; };

		template <PlaneSide a, PlaneSide b, PlaneSide c>
		glm::vec3 Intersection(const glm::vec3* crosses) const;
		
		glm::vec4 m_planes[Count];
		glm::vec3 m_points[8];
	};

	inline bool IsAABBVisible(const AABB_t& aabb, const Frustum& frustum) { return !CVar_EnableCulling.Get() || frustum.IsBoxVisible(aabb.min, aabb.max); }

	///////////////////////////////////////////////////////////////////////////////////////
	// AABB_t
	///////////////////////////////////////////////////////////////////////////////////////

	inline void AABB_t::BuildFromCenterAndExtent(const glm::vec3& center, const glm::vec3& extent)
	{
		min = center - (extent * 0.5f);
		max = center + (extent * 0.5f);
	}

	inline AABB_t AABB_t::ApplyTransform(const glm::mat4& transform) const
	{
		glm::vec3 center = glm::vec3(transform * glm::vec4(GetCenter(), 1));
		glm::vec3 localExtent = GetExtent();

		glm::vec3 right = math::GetRightFromTransform(transform) * localExtent.x;
		glm::vec3 up = math::GetUpFromTransform(transform) * localExtent.y;
		glm::vec3 forward = math::GetForwardFromTransform(transform) * localExtent.z;

		const float newIi = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));

		const float newIj = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));

		const float newIk = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

		AABB_t res;
		res.BuildFromCenterAndExtent(center, { newIi, newIj, newIk });
		return res;
	}

	///////////////////////////////////////////////////////////////////////////////////////
	// Frustum
	///////////////////////////////////////////////////////////////////////////////////////

	inline Frustum::Frustum(const glm::mat4& viewProjectionMatrix)
	{
		glm::mat4 m = glm::transpose(viewProjectionMatrix);
		m_planes[Left] = m[3] + m[0];
		m_planes[Right] = m[3] - m[0];
		m_planes[Bottom] = m[3] + m[1];
		m_planes[Top] = m[3] - m[1];
		m_planes[Near] = m[3] + m[2];
		m_planes[Far] = m[3] - m[2];

		glm::vec3 crosses[Combinations] = {
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Right])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Bottom])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Top])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Bottom])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Top])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Bottom]), glm::vec3(m_planes[Top])),
			glm::cross(glm::vec3(m_planes[Bottom]), glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Bottom]), glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Top]),    glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Top]),    glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Near]),   glm::vec3(m_planes[Far]))
		};

		m_points[0] = Intersection<Left, Bottom, Near>(crosses);
		m_points[1] = Intersection<Left, Top, Near>(crosses);
		m_points[2] = Intersection<Right, Bottom, Near>(crosses);
		m_points[3] = Intersection<Right, Top, Near>(crosses);
		m_points[4] = Intersection<Left, Bottom, Far>(crosses);
		m_points[5] = Intersection<Left, Top, Far>(crosses);
		m_points[6] = Intersection<Right, Bottom, Far>(crosses);
		m_points[7] = Intersection<Right, Top, Far>(crosses);
	}

	// http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
	inline bool Frustum::IsBoxVisible(const glm::vec3& minp, const glm::vec3& maxp) const
	{
		// check box outside/inside of frustum
		for (int i = 0; i < Count; i++)
		{
			if ((glm::dot(m_planes[i], glm::vec4(minp.x, minp.y, minp.z, 1.0f)) < 0.0) &&
				(glm::dot(m_planes[i], glm::vec4(maxp.x, minp.y, minp.z, 1.0f)) < 0.0) &&
				(glm::dot(m_planes[i], glm::vec4(minp.x, maxp.y, minp.z, 1.0f)) < 0.0) &&
				(glm::dot(m_planes[i], glm::vec4(maxp.x, maxp.y, minp.z, 1.0f)) < 0.0) &&
				(glm::dot(m_planes[i], glm::vec4(minp.x, minp.y, maxp.z, 1.0f)) < 0.0) &&
				(glm::dot(m_planes[i], glm::vec4(maxp.x, minp.y, maxp.z, 1.0f)) < 0.0) &&
				(glm::dot(m_planes[i], glm::vec4(minp.x, maxp.y, maxp.z, 1.0f)) < 0.0) &&
				(glm::dot(m_planes[i], glm::vec4(maxp.x, maxp.y, maxp.z, 1.0f)) < 0.0))
			{
				return false;
			}
		}

		// check frustum outside/inside box
		int out;
		out = 0; for (int i = 0; i < 8; i++) out += ((m_points[i].x > maxp.x) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((m_points[i].x < minp.x) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((m_points[i].y > maxp.y) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((m_points[i].y < minp.y) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((m_points[i].z > maxp.z) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((m_points[i].z < minp.z) ? 1 : 0); if (out == 8) return false;

		return true;
	}

	template <Frustum::PlaneSide a, Frustum::PlaneSide b, Frustum::PlaneSide c>
	inline glm::vec3 Frustum::Intersection(const glm::vec3* crosses) const
	{
		float D = glm::dot(glm::vec3(m_planes[a]), crosses[ij2k<b, c>::k]);
		glm::vec3 res = glm::mat3(crosses[ij2k<b, c>::k], -crosses[ij2k<a, c>::k], crosses[ij2k<a, b>::k]) *
			glm::vec3(m_planes[a].w, m_planes[b].w, m_planes[c].w);
		return res * (-1.0f / D);
	}

}