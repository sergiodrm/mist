#include "Texture.h"
#include "RenderSystem/RenderSystem.h"
#include "Utils/FileSystem.h"
#include "Core/SystemMemory.h"

#include <chrono>
#include "Core/Logger.h"
#include "VulkanRenderEngine.h"
#include "RenderAPI/Device.h"

namespace Mist
{
	Texture::Texture(const render::TextureHandle& deviceTexture)
		: m_deviceTexture(deviceTexture)
	{ }

	Texture Texture::GetUnknownTexture()
    {
		Texture t;
		return t;
    }

    bool Texture::LoadFromFile(const TextureLoader::LoadParams& loadParams)
    {
		TextureLoader* loader = _new TextureLoader(this, loadParams);
		resources::PushLoader(loader);
		return true;
    }

	Texture::TextureLoader::TextureLoader(Texture* owner, const LoadParams& params)
		: m_owner(owner), m_params(params), m_textureData{}
	{
		check(m_owner);
		check(FileSystem::FileExists(m_params.filepath.c_str()));
	}

	resources::IResourceLoader::ProcType Texture::TextureLoader::GetProcType() const
	{
		return ProcType::LoadThread;
	}

	resources::IResourceLoader::ProcType Texture::TextureLoader::ProcessLoadThread()
	{
		if (!rendersystem::textureloader::LoadTextureData_u8(&m_textureData, m_params.filepath, m_params.flipVertical))
		{
			logferror("Failed loading texture from: %s\n", m_params.filepath);
			return ProcType::Finished;
		}
		return ProcType::MainThread;
	}

	resources::IResourceLoader::ProcType Texture::TextureLoader::ProcessMainThread()
	{
		check(m_textureData.u8data);
		check(m_textureData.width && m_textureData.height && m_textureData.depth);
		check(m_params.format != render::Format_Undefined);
		render::TextureDescription desc;
		desc.extent = { m_textureData.width, m_textureData.height, m_textureData.depth };
		desc.format = m_params.format;
		desc.debugName = m_params.filepath;
		desc.mipLevels = m_params.calculateMipLevels ? rendersystem::CalculateMipLevels(m_textureData.width, m_textureData.height) : 1;

		// TODO: device as param
		render::Device* device = g_device;
		render::TextureHandle texture = device->CreateTexture(desc);

		render::utils::UploadContext uploadContext(device);
		uploadContext.WriteTexture(texture, 0, 0, m_textureData.u8data, m_textureData.width * m_textureData.height * sizeof(uint8_t) * m_textureData.channels);
		uploadContext.SetTextureLayout(texture, render::ImageLayout_ShaderReadOnly, 0, 0);
		if (m_params.calculateMipLevels)
			rendersystem::GenerateMipMaps(device, texture, &uploadContext);

		rendersystem::textureloader::FreeTextureData(m_textureData);

		check(m_owner && !m_owner->m_deviceTexture);
		m_owner->m_deviceTexture = texture;

		return ProcType::Finished;
	}
}