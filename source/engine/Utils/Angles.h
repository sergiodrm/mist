#pragma once

#include "Core/Types.h"
#include <glm/glm.hpp>

namespace Mist
{
	
	class tAngles
	{
	public:
		tAngles() {}
		tAngles(float v) : m_pitch(v), m_yaw(v), m_roll(v) {}
		tAngles(float pitch, float yaw, float roll)
			: m_roll(roll), m_pitch(pitch), m_yaw(yaw) {}
		explicit tAngles(const glm::vec3& v)
			: m_roll(v[2]), m_pitch(v[0]), m_yaw(v[1]) {}

		void Set(float pitch, float yaw, float roll)
		{
			m_roll = roll;
			m_pitch = pitch;
			m_yaw = yaw;
		}

		float operator[](uint32_t index) const 
		{ 
			check(index < 3);
			return m_angles[index]; 
		}
		float& operator[](uint32_t index)
		{
			check(index < 3);
			return m_angles[index];
		}

		tAngles		operator-() const					{ return tAngles(-m_pitch, -m_yaw, -m_roll); }
		tAngles&	operator=(const tAngles& a)			{ Set(a.m_pitch, a.m_yaw, a.m_roll); return *this; }
		tAngles		operator+(const tAngles& a) const	{ return tAngles(m_pitch + a.m_pitch, m_yaw + a.m_yaw, m_roll + a.m_roll); }
		tAngles&	operator+=(const tAngles& a)		{ Set(m_pitch + a.m_pitch, m_yaw + a.m_yaw, m_roll + a.m_roll); return *this; }
		tAngles		operator-(const tAngles& a) const	{ return tAngles(m_pitch - a.m_pitch, m_yaw - a.m_yaw, m_roll - a.m_roll); }
		tAngles&	operator-=(const tAngles& a)		{ Set(m_pitch - a.m_pitch, m_yaw - a.m_yaw, m_roll - a.m_roll); return *this; }
		tAngles		operator*(float v) const			{ return tAngles(m_pitch * v, m_yaw * v, m_roll * v); }
		tAngles&	operator*=(float v)					{ Set(m_pitch * v, m_yaw * v, m_roll * v); return *this; }
		tAngles		operator/(float v) const			
		{ 
			float inv = 1.f / v;
			return tAngles(m_pitch * inv, m_yaw * inv, m_roll * inv); 
		}
		tAngles&	operator/=(float v)					
		{ 
			float inv = 1.f / v;
			Set(m_pitch * inv, m_yaw * inv, m_roll * inv);
			return *this; 
		}

		bool operator==(const tAngles& a) const { return m_pitch == a.m_pitch && m_yaw == a.m_yaw && m_roll == a.m_roll; }
		bool operator!=(const tAngles& a) const { return !(*this == a); }
		bool Compare(const tAngles& a, float epsilon) const
		{
			if (fabsf(m_pitch - a.m_pitch) > epsilon
				|| fabsf(m_yaw - a.m_yaw) > epsilon
				|| fabsf(m_roll - a.m_roll) > epsilon)
				return false;
			return true;
		}

		glm::mat3 ToMat3() const;
		glm::mat4 ToMat4() const;
		const float* ToFloat() const { return m_angles; }
		float* ToFloat() { return m_angles; }

		// roll pitch yaw stored in degrees.
		union
		{
			float m_angles[3];
			struct  
			{
				float m_pitch;
				float m_yaw;
				float m_roll;
			};
		};
	};
}