#pragma once

#include "Mesh.h"

namespace Mist
{
	struct RenderObject
	{
		enum : uint32_t { InvalidId = UINT32_MAX };
		uint32_t Id{ InvalidId };

		RenderObject() = default;
		RenderObject(uint32_t id) : Id(id) {}
		inline bool IsValid() const { return Id != InvalidId; }
		inline void Invalidate() { Id = InvalidId; }
		operator uint32_t() const { return Id; }
	};
}
