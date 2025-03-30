#pragma once

#include "Core/Types.h"

#define PROFILE_SCOPE_LOG(id, msg) Mist::tScopeProfiler __scopeprof_##id(msg)
#define PROFILE_SCOPE_LOGF(id, fmt, ...) Mist::tScopeProfiler __scopeprof_##id##; __scopeprof_##id##.m_msg.Fmt(fmt, __VA_ARGS__); __scopeprof_##id##.m_start = Mist::GetTimePoint()

namespace Mist
{
	typedef long long tTimePoint;
	tTimePoint GetTimePoint();
	float GetMiliseconds(tTimePoint point);

	class tScopeProfiler
	{
	public:
		tScopeProfiler();
		tScopeProfiler(const char* msg);
		~tScopeProfiler();
		tTimePoint m_start;
		tFixedString<256> m_msg;
	};

	class FixTickTimer
	{
	public:
		FixTickTimer(float fixStepMs = 0.016f);

		void SetFixStep(float fixStepMs);
		inline float GetFixStepMs() const { return m_fixStep; }
		inline float GetTotalTime() const { return m_totalTime; }
		inline float GetFrameTime() const { return m_frameTime; }
		inline float GetLastDiffTime() const { return m_lastDiffTime; }

		void ProcessTimePoint();

		bool CanTickAgain() const;
		void AdvanceTime();


	private:
		tTimePoint m_point;
		float m_fixStep;
		float m_totalTime;
		float m_frameTime;
		float m_lastDiffTime;
	};
}
