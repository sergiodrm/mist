#pragma once

#define PROFILE_SCOPE_LOG(id, msg) Mist::tScopeProfiler __scopeprof_##id(msg)

namespace Mist
{
	typedef long long tTimePoint;
	tTimePoint GetTimePoint();
	float GetMiliseconds(tTimePoint point);

	class tScopeProfiler
	{
	public:
		tScopeProfiler(const char* msg);
		~tScopeProfiler();
	private:
		tTimePoint m_start;
		char m_msg[128];
	};
}
