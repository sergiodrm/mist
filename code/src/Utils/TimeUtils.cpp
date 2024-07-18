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

	tScopeProfiler::tScopeProfiler(const char* msg)
	{
		strcpy_s(m_msg, msg);
		m_start = GetTimePoint();
	}

	tScopeProfiler::~tScopeProfiler()
	{
		float ms = GetMiliseconds(GetTimePoint() - m_start);
		Mist::Logf(Mist::LogLevel::Debug, "%s [time lapsed: %f ms]\n", m_msg, ms);
	}
}