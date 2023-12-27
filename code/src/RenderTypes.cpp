#include "RenderTypes.h"
#include "RenderHandle.h"
#include "InitVulkanTypes.h"
#include "VulkanRenderEngine.h"
#include "Debug.h"

template <>
struct std::hash<vkmmc::RenderHandle>
{
	std::size_t operator()(const vkmmc::RenderHandle& key) const
	{
		return hash<uint32_t>()(key.Handle);
	}
};

namespace vkmmc
{
	
	
}
