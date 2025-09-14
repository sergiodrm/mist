#pragma once

#include "RenderAPI/Device.h"

namespace rendersystem
{
    namespace ui
    {
        typedef void(*ImGuiWindowCallback)(void*);
        typedef void(*ImGuiMenuCallback)();

        void Init(render::Device* device, render::RenderTargetHandle rt, void* windowHandle);
        void Destroy();
        void BeginFrame();
        void EndFrame(render::CommandListHandle cmd);

        void Show();

        void AddWindowCallback(const char* id, ImGuiWindowCallback fn, void* userData = nullptr, bool openByDefault = false);
        void AddMenuCallback(const char* id, ImGuiMenuCallback fn);
    }
}