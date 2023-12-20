// header file for vkmmc project 

#pragma once

#include "RenderTypes.h"
#include "RenderDescriptor.h"

namespace vkmmc
{
	namespace io
	{
		struct TextureRaw
		{
			unsigned char* Pixels = nullptr;
			uint32_t Width = 0;
			uint32_t Height = 0;
			uint32_t Channels = 0;
		};
		bool LoadTexture(const char* path, TextureRaw& out);
		void FreeTexture(unsigned char* data);
	}

	enum EImageFormat
	{
		IMAGE_FORMAT_R8G8B8,
		IMAGE_FORMAT_R8G8B8A8,
		IMAGE_FORMAT_INVALID = 0xffff
	};

	struct RenderTextureCreateInfo
	{
		RenderContext RContext;
		io::TextureRaw Raw;
		std::function<void(std::function<void(VkCommandBuffer)>)> RecordCommandRutine;
	};

	class RenderTexture
	{
	public:
		void Init(const RenderTextureCreateInfo& info);
		void Destroy(const RenderContext& renderContext);

		VkImageView GetImageView() const { return m_imageView; }
	private:
		AllocatedImage m_image;
		VkImageView m_imageView;
	};

	struct RenderTextureDescriptorCreateInfo
	{
		RenderContext RContext;
		VkDescriptorSet Descriptor; // Previous allocated descriptor to be updated with texture.
		uint32_t Binding;
		uint32_t ArrayElement;
		VkImageView ImageView;
	};

	class RenderTextureDescriptor
	{
	public:
		void Init(const RenderTextureDescriptorCreateInfo& info);
		void Destroy(const RenderContext& renderContext);

		void Bind(VkCommandBuffer cmd, class RenderPipeline pipeline) const;
	private:
		VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
		VkSampler m_sampler = VK_NULL_HANDLE;
	};

}