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

	struct RenderTextureCreateInfo
	{
		RenderContext RContext;
		io::TextureRaw Raw;
	};

	class Texture
	{
	public:
		void Init(const RenderTextureCreateInfo& info);
		void Destroy(const RenderContext& renderContext);

		VkImageView GetImageView() const { return m_imageView; }
		void Bind(const RenderContext& renderContext, VkDescriptorSet set, VkSampler sampler, uint32_t binding, uint32_t arrayIndex = 0) const;
	private:
		AllocatedImage m_image;
		VkImageView m_imageView;
	};
}