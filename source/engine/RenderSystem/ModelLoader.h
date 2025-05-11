#pragma once

#include "Core/Types.h"
#include "RenderAPI/Types.h"

namespace render
{
    class Device;
}

namespace rendersystem
{
    struct Model;

    namespace modelloader
    {
        enum AttributeType
        {
            Attribute_Position,
            Attribute_Normal,
            Attribute_TexCoord0,
            Attribute_TexCoord1,
            Attribute_Tangent,
            Attribute_VertexColor,
            Attribute_Count
        };

        inline uint32_t GetAttributeElementCount(AttributeType type)
        {
            switch (type)
            {
            case Attribute_Position: return 3;
            case Attribute_Normal: return 3;
            case Attribute_TexCoord0: return 2;
            case Attribute_TexCoord1: return 2;
            case Attribute_Tangent: return 4;
            case Attribute_VertexColor: return 4;
            case Attribute_Count:
            default:
                break;
            }
            return 0;
        }

        inline render::Format GetAttributeFormat(AttributeType type)
        {
            switch (type)
            {
            case Attribute_Position: return render::Format_R32G32B32_SFloat;
            case Attribute_Normal: return render::Format_R32G32B32_SFloat;
            case Attribute_TexCoord0: return render::Format_R32G32_SFloat;
            case Attribute_TexCoord1: return render::Format_R32G32_SFloat;
            case Attribute_Tangent: return render::Format_R32G32B32A32_SFloat;
            case Attribute_VertexColor: return render::Format_R32G32B32A32_SFloat;
            case Attribute_Count:
            default:
                break;
            }
            return render::Format_Undefined;
        }

        struct VertexLayout
        {
            uint8_t attributeMask = 0;
            uint32_t offsets[Attribute_Count];
            uint32_t size = 0;
        };

        bool LoadModelFromFile(render::Device* device, const char* filepath, Model* outModel);
    }
}