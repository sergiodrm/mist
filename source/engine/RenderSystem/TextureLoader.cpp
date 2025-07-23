#include "TextureLoader.h"
#include "Utils/FileSystem.h"
#include "Utils/TimeUtils.h"
#include "stb_image.h"
#include "Core/Logger.h"

//#define TEXLOAD_DUMP_INFO
#ifdef TEXLOAD_DUMP_INFO
#define texloadlabel "[texture loader] "
#define texloadlog(fmt) loginfo(texloadlabel fmt)
#define texloadlogf(fmt, ...) logfinfo(texloadlabel fmt, __VA_ARGS__)
#define profile_texload_scope(scope_name, msg) PROFILE_SCOPE_LOG(scope_name, msg)
#define profile_texload_scope_f(scope_name, fmt, ...) PROFILE_SCOPE_LOGF(scope_name, fmt, __VA_ARGS__)
#else
#define texloadlog(fmt) DUMMY_MACRO
#define texloadlogf(fmt, ...) DUMMY_MACRO
#define profile_texload_scope(scope_name, msg) DUMMY_MACRO
#define profile_texload_scope_f(scope_name, fmt, ...) DUMMY_MACRO
#endif

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

            // Restore shader read only layout for dst mip
            uploadContext->SetTextureLayout(texture, render::ImageLayout_ShaderReadOnly, 0, i);

            // Update dimensions
            width = __max(1, width >> 1);
            height = __max(1, height >> 1);
        }
    }

    namespace textureloader
    {
        bool LoadTextureData_u8(TextureData* out, const char* filepath, bool flipVertical)
        {
            profile_texload_scope_f(LoadTextureData_u8, "LoadTexture_u8 (%s)", filepath);
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

        bool LoadTextureData_f(TextureData* out, const char* filepath, bool flipVertical)
        {
            profile_texload_scope_f(LoadTextureData_f, "LoadTextureData_f (%s)", filepath);
            check(out);
            Mist::cAssetPath assetPath(filepath);
            stbi_set_flip_vertically_on_load(flipVertical);
            int32_t width, height, channels;
			float* pixels = stbi_loadf(assetPath, &width, &height, &channels, STBI_rgb_alpha);
			if (!pixels)
			{
				return false;
			}
			if (channels != 4)
				channels = 4;
			out->fdata = pixels;
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
            profile_texload_scope_f(LoadTextureFromFile, "Load texture from file (%s)", filepath);
            check(textureOut && device);

            TextureData data{};
            if (!LoadTextureData_u8(&data, filepath, flipVertical))
            {
                logferror("Failed to load texture data from %s.\n", filepath);
                return false;
            }

            profile_texload_scope_f(CreateAndFillTexture, "Create and fill texture from file (%s)", filepath);
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
            uploadContext->WriteTexture(texture, 0, 0, data.u8data, data.width * data.height * sizeof(stbi_uc) * data.channels);
            uploadContext->SetTextureLayout(texture, render::ImageLayout_ShaderReadOnly, 0, 0);
            if (calculateMipLevels)
                GenerateMipMaps(device, texture, uploadContext);

            FreeTextureData(data);
            return true;
        }

        bool LoadHDRTextureFromFile(render::TextureHandle* textureOut, render::Device* device, const char* filepath, bool flipVertical, render::utils::UploadContext* uploadContext)
        {
            profile_texload_scope_f(LoadHDRTextureFromFile, "Load HDR texture from file (%s)", filepath);
            TextureData data{};
			if (!LoadTextureData_f(&data, filepath, flipVertical))
			{
				logferror("Failed to load texture data from %s.\n", filepath);
				return false;
			}
            check(data.channels == 4);

			profile_texload_scope_f(CreateAndFillTexture, "Create and fill texture from file (%s)", filepath);
            render::Extent3D extent = { data.width, data.height, 1 };
			render::TextureDescription desc;
			desc.extent = extent;
            desc.format = render::Format_R32G32B32A32_SFloat;
			desc.debugName = filepath;
            desc.layers = 1;
			desc.mipLevels = 1;
			render::TextureHandle texture = device->CreateTexture(desc);
			(*textureOut) = texture;

			render::utils::UploadContext upload;
			if (!uploadContext)
			{
				uploadContext = &upload;
				uploadContext->Init(device);
			}
			uploadContext->WriteTexture(texture, 0, 0, data.fdata, data.width * data.height * sizeof(float) * data.channels);
			uploadContext->SetTextureLayout(texture, render::ImageLayout_ShaderReadOnly, 0, 0);
			
			FreeTextureData(data);
			return true;
        }
    }
}
