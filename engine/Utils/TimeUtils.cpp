#include "Utils/TimeUtils.h"
#include "Core/Debug.h"



#if defined(_WIN32)
#include <windows.h>
#include <winnt.h>
#include <profileapi.h> 
#else
#error SO not supported
#endif
#include "Core/Logger.h"
#include "GenericUtils.h"


namespace Mist
{
	int64_t cpufreq(bool force = false)
	{
#if defined(_WIN32)
		LARGE_INTEGER f;
		BOOL b = QueryPerformanceFrequency(&f);
		check(b && f.QuadPart);
		return f.QuadPart;
#else 
		return 0.f;
#endif // 0
	}

	tTimePoint GetTimePoint()
	{
#if defined(_WIN32)
		LARGE_INTEGER t;
		BOOL b = QueryPerformanceCounter(&t);
		check(b);
		return t.QuadPart;
#else
		return 0;
#endif // defined(_WIN32)
	}

	float GetMiliseconds(tTimePoint point)
	{
		return (float)point / (float)cpufreq() * 1e3f;
	}

	tScopeProfiler::tScopeProfiler()
	{}

	tScopeProfiler::tScopeProfiler(const char* msg)
	{
		m_msg = msg;
		m_start = GetTimePoint();
	}

	tScopeProfiler::~tScopeProfiler()
	{
		float ms = GetMiliseconds(GetTimePoint() - m_start);
		logfinfo("%s [time lapsed: %f ms]\n", m_msg, ms);
	}

	FixTickTimer::FixTickTimer(float fixStepMs)
		: m_fixStep(0.f),
		m_totalTime(0.f),
		m_frameTime(0.f)
	{
		SetFixStep(fixStepMs);
		m_point = GetTimePoint();
	}

	void FixTickTimer::SetFixStep(float fixStepMs)
	{
		m_fixStep = math::Clamp(fixStepMs, 0.001f, FLT_MAX);
	}

	void FixTickTimer::ProcessTimePoint()
	{
		tTimePoint now = GetTimePoint();
		m_lastDiffTime = GetMiliseconds(now - m_point);
		m_point = now;
		m_frameTime += m_lastDiffTime;
		m_totalTime += m_lastDiffTime;
	}

	bool FixTickTimer::CanTickAgain() const
	{
		return m_frameTime >= m_fixStep;
	}

	void FixTickTimer::AdvanceTime()
	{
		m_frameTime = __max(0.f, m_frameTime - m_fixStep);
	}
}