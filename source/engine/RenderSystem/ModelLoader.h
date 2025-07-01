#pragma once

#include "Core/Types.h"
#include "RenderAPI/Types.h"
#if 0

namespace render
{
    class Device;
}

namespace rendersystem
{
    struct Model;

    namespace modelloader
    {
        bool LoadModelFromFile(render::Device* device, const char* filepath, Model* outModel);
    }
}
#endif // 0
