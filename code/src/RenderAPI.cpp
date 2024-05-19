#include "RenderAPI.h"

namespace Mist
{
    namespace Render
    {
        bool WaitFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll, uint32_t timeoutNs)
        {
            VkResult res = vkWaitForFences(device, fenceCount, fences, waitAll, timeoutNs);
            return res == VK_SUCCESS;
        }

        void ResetFences(Device device, const Fence* fences, uint32_t fenceCount)
        {
            vkResetFences(device, fenceCount, fences);
        }
    }
}
