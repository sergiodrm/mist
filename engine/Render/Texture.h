// header file for Mist project 

#pragma once

#include "Render/RenderTypes.h"
#include "Core/Types.h"
#include "Render/RenderAPI.h"
#include "Render/RenderResource.h"


namespace Mist
{
	class cTexture;

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
		ESampleCount SampleCount = SAMPLE_COUNT_1_BIT;
		EImageUsage Usage = IMAGE_USAGE_TRANSFER_SRC_BIT | IMAGE_USAGE_TRANSFER_DST_BIT | IMAGE_USAGE_SAMPLED_BIT;
		EImageLayout InitialLayout = IMAGE_LAYOUT_UNDEFINED;
		VkImageCreateFlags Flags = 0;
		SamplerDescription SamplerDesc;
		tFixedString<128> DebugName;
	};

	struct tViewDescription
	{
		bool UseMipmaps = true;
		uint32_t BaseArrayLayer = 0;
		uint32_t LayerCount = VK_REMAINING_ARRAY_LAYERS;
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
	bool BindDescriptorTexture(const RenderContext& context, cTexture* texture, VkDescriptorSet& set, uint32_t binding, uint32_t arrayIndex);
	bool IsDepthFormat(EFormat format);
	bool IsStencilFormat(EFormat format);


	namespace io
	{
		struct TextureRaw
		{
			unsigned char* Pixels = nullptr;
			uint32_t Width = 0;
			uint32_t Height = 0;
			uint32_t Channels = 0;
		};
		bool LoadTexture(const char* path, TextureRaw& out, bool flipVertical = false);
		void FreeTexture(unsigned char* data);
	}

	

	class cTexture : public cRenderResource<RenderResource_Texture>
	{
		cTexture() = default;
	public:

		static cTexture* Create(const RenderContext& context, const tImageDescription& description);
		static void Destroy(const RenderContext& context, cTexture* texture);

		void SetImageLayers(const RenderContext& context, const uint8_t** layerDataArray, uint32_t layerCount);
		void GenerateMipmaps(const RenderContext& context);

		ImageView CreateView(const RenderContext& context, const tViewDescription& viewDesc);
		ImageView GetView(uint32_t viewIndex) const;
		inline VkImage GetNativeImage() const { return m_image.Image; }
		uint32_t GetViewCount() const;
		Sampler GetSampler() const { return m_sampler; }
		void SetSampler(Sampler sampler) { m_sampler = sampler; }
		const tImageDescription& GetDescription() const { return m_description; }

		inline EImageLayout GetImageLayout() const { return m_layout; }
		// used for render target for update layout after using texture as attachment. Must not be use to change layout tracking.
		inline void SetImageLayout(EImageLayout layout) { m_layout = layout; }
		void TransferImageLayout(const RenderContext& context, EImageLayout dstLayout,
			VkAccessFlags srcAccess = 0, VkAccessFlags dstAccess = 0);

	private:
		void Destroy(const RenderContext& context);
		void AllocateImage(const RenderContext& context, const tImageDescription& desc);

		tImageDescription m_description;
		AllocatedImage m_image;
		tDynArray<ImageView> m_views;
		Sampler m_sampler;
		EImageLayout m_layout;
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


	bool LoadTextureFromFile(const RenderContext& context, const char* filepath, cTexture** texture, EFormat format);

}