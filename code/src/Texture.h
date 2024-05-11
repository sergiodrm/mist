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

	

	class Sampler
	{
	public:
		Sampler() : m_sampler(VK_NULL_HANDLE) {}
		Sampler(VkSampler sampler) : m_sampler(sampler) {}
		Sampler(const Sampler& other) : m_sampler(other.m_sampler) {}
		Sampler& operator=(const Sampler& other) { m_sampler = other.m_sampler; return *this; }
		void Destroy(const RenderContext& renderContext);
		VkSampler GetSampler() const { return m_sampler; }
	private:
		VkSampler m_sampler;
	};

	class SamplerBuilder
	{
	public:
		EFilterType MinFilter = FILTER_LINEAR;
		EFilterType MagFilter = FILTER_LINEAR;
		union
		{
			struct  
			{
				ESamplerAddressMode U;
				ESamplerAddressMode V;
				ESamplerAddressMode W;
			} AddressMode;
			ESamplerAddressMode AddresModeUVW[3];
		} AddressMode = { SAMPLER_ADDRESS_MODE_REPEAT, SAMPLER_ADDRESS_MODE_REPEAT, SAMPLER_ADDRESS_MODE_REPEAT };

		SamplerBuilder() = default;
		Sampler Build(const RenderContext& renderContext) const;
	};
}