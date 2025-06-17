#include "TextureLoader.h"
#include "Utils/FileSystem.h"
#include "Utils/TimeUtils.h"
#include "stb_image.h"
#include "Core/Logger.h"

namespace rendersystem
{
    uint32_t CalculateMipLevels(uint32_t width, uint32_t height)
    {
        return (uint32_t)floorf(log2f((float)__max(width, height))) + 1;
    }

    void GenerateMipMaps(render::Device* device, render::TextureHandle texture, render::utils::UploadContext* uploadContext)
    {
        // Check if image format supports linear filtering.
        check(device->GetContext().FormatSupportsLinearFiltering(texture->m_description.format) && "Unsupported format to generate mipmap levels.");

        render::utils::UploadContext upload;
        if (!uploadContext)
        {
            uploadContext = &upload;
            upload.Init(device);
        }

        int32_t width = static_cast<int32_t>(texture->m_description.extent.width);
        int32_t height = static_cast<int32_t>(texture->m_description.extent.height);
        for (uint32_t i = 1; i < texture->m_description.mipLevels; ++i)
        {
            /**
			 * Message: vkCmdBlitImage(): pRegions[0] srcOffsets[0].z is 0 and srcOffsets[1].z is 0 but srcImage is VK_IMAGE_TYPE_2D.
                The Vulkan spec states: If srcImage is of type VK_IMAGE_TYPE_1D or VK_IMAGE_TYPE_2D, then for each element of pRegions, srcOffsets[0].z must be 0 and srcOffsets[1].z must be 1 
                (https://vulkan.lunarg.com/doc/view/1.4.313.1/windows/antora/spec/latest/chapters/copies.html#VUID-vkCmdBlitImage-srcImage-00247)
             */
            render::BlitDescription blit;
            blit.src = texture;
            blit.dst = texture;
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { width, height, 1 };
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { __max(1, width >> 1), __max(1, height >> 1), 1 };
            blit.filter = render::Filter_Linear;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.layer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.layer = 0;
            blit.dstSubresource.layerCount = 1;
            uploadContext->Blit(blit);

            // Update dimensions
            width = __max(1, width >> 1);
            height = __max(1, height >> 1);
        }
    }

    namespace textureloader
    {
        bool LoadTextureData_u8(TextureData* out, const char* filepath, bool flipVertical)
        {
            PROFILE_SCOPE_LOGF(LoadTextureData_u8, "LoadTexture_u8 (%s)", filepath);
            check(out);
            Mist::cAssetPath assetPath(filepath);
            stbi_set_flip_vertically_on_load(flipVertical);
            int32_t width, height, channels;
            stbi_uc* pixels = stbi_load(assetPath, &width, &height, &channels, STBI_rgb_alpha);
            if (!pixels)
            {
                FreeTextureData(*out);
                return false;
            }
            if (channels != 4)
                channels = 4;
            out->u8data = pixels;
            out->width = Mist::limits_cast<uint32_t>(width);
            out->height = Mist::limits_cast<uint32_t>(height);
            out->depth = 1;
            out->channels = Mist::limits_cast<uint32_t>(channels);
            return true;
        }

        void FreeTextureData(TextureData& data)
        {
            if (data.u8data)
                stbi_image_free(data.u8data);
            memset(&data, 0, sizeof(TextureData));
        }

        bool LoadTextureFromFile(render::TextureHandle* textureOut, render::Device* device, const char* filepath, bool flipVertical, bool calculateMipLevels, render::utils::UploadContext* uploadContext)
        {
            PROFILE_SCOPE_LOGF(LoadTextureFromFile, "Load texture from file (%s)", filepath);
            check(textureOut && device);

            TextureData data;
            if (!LoadTextureData_u8(&data, filepath, flipVertical))
            {
                logferror("Failed to load texture data from %s.\n", filepath);
                return false;
            }

            PROFILE_SCOPE_LOGF(CreateAndFillTexture, "Create and fill texture from file (%s)", filepath);
            render::TextureDescription desc;
            desc.extent = { data.width, data.height, 1 };
            desc.format = render::Format_R8G8B8A8_UNorm;
            desc.debugName = filepath;
            desc.mipLevels = calculateMipLevels ? CalculateMipLevels(data.width, data.height) : 1;
            render::TextureHandle texture = device->CreateTexture(desc);
            (*textureOut) = texture;

            render::utils::UploadContext upload;
            if (!uploadContext)
            {
                uploadContext = &upload;
                uploadContext->Init(device);
            }
            upload.WriteTexture(texture, 0, 0, data.u8data, data.width * data.height * sizeof(stbi_uc) * data.channels);
            if (calculateMipLevels)
                GenerateMipMaps(device, texture, uploadContext);

            FreeTextureData(data);
            return true;
        }
    }
}
