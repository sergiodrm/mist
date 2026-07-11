#pragma once

#include "Core/Types.h"
#include "RenderAPI/Types.h"

namespace render
{
    class Device;
}

namespace Mist
{
    class cModel;

    namespace modelloader
    {
        bool LoadModelFromFile(render::Device* device, const char* filepath, cModel* outModel);
    }
}
