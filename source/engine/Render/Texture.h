#pragma once

#include "RenderAPI/Device.h"
#include "RenderSystem/TextureLoader.h"
#include "Core/Atomic.h"
#include "Core/Thread.h"
#include "Core/Mutex.h"

#include "Utils/FileSystem.h"
#include "Resources/ResourceLoader.h"

namespace rendersystem
{
    class RenderSystem;
}

namespace Mist
{
    /**
     * Wrapper for device texture. This class can process a load task in a worker thread meanwhile returns a default texture.
     * Once the final texture is ready, the GetDeviceTexture() method will return the right texture loaded from the file.
     */
    class Texture
    {
    public:

        class TextureLoader : public resources::IResourceLoader
        {
        public:

            struct LoadParams
            {
                cAssetPath filepath;
                bool flipVertical = false;
                bool calculateMipLevels = true;
                render::Format format = render::Format_Undefined;
            };

            TextureLoader(Texture* owner, const LoadParams& params);
            virtual ProcType GetProcType() const override;
            virtual ProcType ProcessLoadThread() override;
            virtual ProcType ProcessMainThread() override;
        private:
            Texture* m_owner;
            LoadParams m_params;
            rendersystem::textureloader::TextureData m_textureData;
        };

        Texture() = default;
        Texture(const render::TextureHandle& deviceTexture);

        static Texture GetUnknownTexture();

        bool LoadFromFile(const TextureLoader::LoadParams& loadParams);

        const render::TextureHandle& GetDeviceTexture() const { return m_deviceTexture; }

    private:
        render::TextureHandle m_deviceTexture{ nullptr };
    };
}