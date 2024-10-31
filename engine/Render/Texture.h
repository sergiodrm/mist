// header file for Mist project 

#pragma once

#include "Render/RenderTypes.h"
#include "Core/Types.h"
#include "Render/RenderAPI.h"


namespace Mist
{
	class Texture;

	struct SamplerDescription
	{
		EFilterType MinFilter = FILTER_LINEAR;
		EFilterType MagFilter = FILTER_LINEAR;
		ESamplerAddressMode AddressModeU = SAMPLER_ADDRESS_MODE_REPEAT;
		ESamplerAddressMode AddressModeV = SAMPLER_ADDRESS_MODE_REPEAT;
		ESamplerAddressMode AddressModeW = SAMPLER_ADDRESS_MODE_REPEAT;
		VkBool32 AnisotropyEnable;
		ESamplerMipmapMode MipmapMode = SAMPLER_MIPMAP_MODE_LINEAR;
		float MaxAnisotropy;
		float MipLodBias = 0.f;
		float MinLod = 0.f;
		float MaxLod = -1.f;
	};

	struct tImageDescription
	{
		EFormat Format;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipLevels = 1;
		uint32_t Depth = 1;
		uint32_t Layers = 1;
		ESampleCountBit SampleCount = SAMPLE_COUNT_1_BIT;
		EImageUsage Usage = IMAGE_USAGE_TRANSFER_SRC_BIT | IMAGE_USAGE_TRANSFER_DST_BIT | IMAGE_USAGE_SAMPLED_BIT;
		VkImageCreateFlags Flags = 0;
		SamplerDescription SamplerDesc;
	};

	struct tViewDescription
	{
		bool UseMipmaps = true;
		uint32_t BaseArrayLayer = 0;
		uint32_t LayerCount = 1;
		EImageAspect AspectMask = IMAGE_ASPECT_COLOR_BIT;
		VkImageViewType ViewType = VK_IMAGE_VIEW_TYPE_2D;
		VkImageViewCreateFlags Flags = 0;
	};


	Sampler CreateSampler(const RenderContext& renderContext, const SamplerDescription& description);
	Sampler CreateSampler(const RenderContext& renderContext,
		EFilterType minFilter = FILTER_LINEAR,
		EFilterType magFilter = FILTER_LINEAR,
		ESamplerAddressMode modeU = SAMPLER_ADDRESS_MODE_REPEAT,
		ESamplerAddressMode modeV = SAMPLER_ADDRESS_MODE_REPEAT,
		ESamplerAddressMode modeW = SAMPLER_ADDRESS_MODE_REPEAT);
	uint32_t GetSamplerPackedDescription(const SamplerDescription& desc);
	void DestroySamplers(const RenderContext& renderContext);

	bool CreateImage(const RenderContext& context, const tImageDescription& createInfo, AllocatedImage& imageOut);
	uint32_t CalculateMipLevels(uint32_t width, uint32_t height);
	bool TransitionImageLayout(const RenderContext& context, const AllocatedImage& image, EImageLayout oldLayout, EImageLayout newLayout, uint32_t mipLevels);
	bool TransferImage(const RenderContext& context, const tImageDescription& createInfo, const uint8_t** layerArray, uint32_t layerCount, AllocatedImage& imageOut);
	bool GenerateImageMipmaps(const RenderContext& context, AllocatedImage& image, const tImageDescription& imageDesc);
	bool BindDescriptorTexture(const RenderContext& context, Texture* texture, VkDescriptorSet& set, uint32_t binding, uint32_t arrayIndex);
	bool IsDepthFormat(EFormat format);


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

	

	class Texture
	{
		Texture() = default;
	public:

		static Texture* Create(const RenderContext& context, const tImageDescription& description);
		static void Destroy(const RenderContext& context, Texture* texture);

		void SetImageLayers(const RenderContext& context, const uint8_t** layerDataArray, uint32_t layerCount);
		void GenerateMipmaps(const RenderContext& context);

		ImageView CreateView(const RenderContext& context, const tViewDescription& viewDesc);
		ImageView GetView(uint32_t viewIndex) const;
		uint32_t GetViewCount() const;
		Sampler GetSampler() const { return m_sampler; }
		const tImageDescription& GetDescription() const { return m_description; }

		void SetDebugName(const RenderContext& context, const char* str);

	private:
		void Destroy(const RenderContext& context);
		void AllocateImage(const RenderContext& context, const tImageDescription& desc);

		tImageDescription m_description;
		AllocatedImage m_image;
		tDynArray<ImageView> m_views;
		Sampler m_sampler;
		char m_debugName[64];
	};
	

	struct TextureBindingDescriptor
	{
		ImageView View;
		EImageLayout Layout;
		Sampler Sampler;
	};
#if 0

	struct TextureCreateInfo
	{
		EFormat Format;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Depth = 1;
		bool Mipmaps = true;

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
		ImageView m_imageView;
		Sampler m_sampler;
	};
#endif // 0


	bool LoadTextureFromFile(const RenderContext& context, const char* filepath, Texture** texture, EFormat format);

}