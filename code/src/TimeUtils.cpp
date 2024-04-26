#include "TimeUtils.h"
#include <ctime>

namespace Mist
{
	tTimePoint GetTimePoint()
	{
		return clock();
	}

	float GetMiliseconds(tTimePoint point)
	{
		return ((float)point / (float)CLOCKS_PER_SEC) * 1e3f;
	}
}