#pragma once

#include <cstdint>
#include <type_traits>
#include <xhash>


namespace vkmmc
{
	enum { InvalidRenderHandle = UINT32_MAX };
	struct RenderHandle
	{
		uint32_t Handle = InvalidRenderHandle;
		RenderHandle() = default;
		RenderHandle(uint32_t h) : Handle(h) {}
		inline bool IsValid() const { return Handle != InvalidRenderHandle; }
		operator uint32_t() const { return Handle; }
		size_t Hash() const { return std::hash<uint32_t>()(Handle); }
		inline bool operator==(const RenderHandle& r) const { return Handle == r.Handle; }
		inline bool operator!=(const RenderHandle& r) const { return !(*this == r); }

		struct Hasher
		{
			std::size_t operator()(const RenderHandle& key) const
			{
				return key.Hash();
			}
		};
	};
}
