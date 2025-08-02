#pragma once

#include "RenderAPI/Device.h"

namespace rendersystem
{
    class RenderSystem;

    uint32_t CalculateMipLevels(uint32_t width, uint32_t height);
    void GenerateMipMaps(const render::CommandListHandle& cmd, const render::TextureHandle& texture);
    void GenerateMipMaps(render::Device* device, render::TextureHandle texture, render::utils::UploadContext* uploadContext = nullptr);

    namespace textureloader
    {
        struct TextureData
        {
            union
            {
                uint8_t* u8data;
                float* fdata;
            };
            uint32_t width;
            uint32_t height;
            uint32_t depth;
            uint32_t channels;
        };

        bool LoadTextureData_u8(TextureData* out, const char* filepath, bool flipVertical = false);
        bool LoadTextureData_f(TextureData* out, const char* filepath, bool flipVertical = false);
        void FreeTextureData(TextureData& data);

        bool LoadTextureFromFile(render::TextureHandle* textureOut, render::Device* device, const char* filepath, bool flipVertical = false, bool calculateMipLevels = true, render::utils::UploadContext* uploadContext = nullptr);
        bool LoadHDRTextureFromFile(render::TextureHandle* textureOut, render::Device* device, const char* filepath, bool flipVertical = false, render::utils::UploadContext* uploadContext = nullptr);
    }
}