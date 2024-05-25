// header file for Mist project 

#pragma once

#include "RenderTypes.h"

namespace Mist
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

	struct TextureCreateInfo
	{
		EFormat Format;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Depth = 1;

		void* Pixels = nullptr;
		uint32_t PixelCount = 0;
	};

	class Texture
	{
	public:
		void Init(const RenderContext& renderContext, const TextureCreateInfo& textureInfo);
		void Destroy(const RenderContext& renderContext);

		VkImageView GetImageView() const { return m_imageView; }
		void Bind(const RenderContext& renderContext, VkDescriptorSet set, VkSampler sampler, uint32_t binding, uint32_t arrayIndex = 0) const;
	private:
		AllocatedImage m_image;
		VkImageView m_imageView;
	};

	struct SamplerDescription
	{
		EFilterType MinFilter = FILTER_LINEAR;
		EFilterType MagFilter = FILTER_LINEAR;
		ESamplerAddressMode AddressModeU = SAMPLER_ADDRESS_MODE_REPEAT;
		ESamplerAddressMode AddressModeV = SAMPLER_ADDRESS_MODE_REPEAT;
		ESamplerAddressMode AddressModeW = SAMPLER_ADDRESS_MODE_REPEAT;
	};

	typedef VkSampler Sampler;

	Sampler CreateSampler(const RenderContext& renderContext, const SamplerDescription& description);
	Sampler CreateSampler(const RenderContext& renderContext, 
		EFilterType minFilter = FILTER_LINEAR, 
		EFilterType magFilter = FILTER_LINEAR, 
		ESamplerAddressMode modeU = SAMPLER_ADDRESS_MODE_REPEAT, 
		ESamplerAddressMode modeV = SAMPLER_ADDRESS_MODE_REPEAT, 
		ESamplerAddressMode modeW = SAMPLER_ADDRESS_MODE_REPEAT);
	uint32_t GetSamplerPackedDescription(const SamplerDescription& desc);
	void DestroySamplers(const RenderContext& renderContext);
}