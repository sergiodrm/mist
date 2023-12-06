#pragma once

#include <cstdint>
#include <type_traits>


namespace vkmmc
{
	enum { InvalidRenderHandle = UINT32_MAX };
	struct RenderHandle
	{
		uint32_t Handle = InvalidRenderHandle;
		inline bool IsValid() const { return Handle != InvalidRenderHandle; }
		operator uint32_t() const { return Handle; }
		inline bool operator==(const RenderHandle& r) const { return Handle == r.Handle; }
		inline bool operator!=(const RenderHandle& r) const { return !(*this == r); }
	};

	
	
}
