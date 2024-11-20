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
		tFixedString<128> m_msg;
	};
}
