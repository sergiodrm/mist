#pragma once

#include "RenderAPI/Device.h"

namespace rendersystem
{
    namespace ui
    {
        void Init(render::Device* device, render::RenderTargetHandle rt, void* windowHandle);
        void Destroy();
        void BeginFrame();
        void EndFrame(render::CommandListHandle cmd);
    }
}