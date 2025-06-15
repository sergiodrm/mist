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

		inline bool operator==(const SamplerDescription& other) const
		{
			return !memcmp(this, &other, sizeof(SamplerDescription));
		}
	};

	struct ImageDescription
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
		String DebugName;

		inline bool operator==(const ImageDescription& other) const 
		{ 
			return Format == other.Format
				&& Width == other.Width
				&& Height == other.Height
				&& Depth == other.Depth
				&& MipLevels == other.MipLevels
				&& Layers == other.Layers
				&& SampleCount == other.SampleCount
				&& Usage == other.Usage
				&& InitialLayout == other.InitialLayout
				&& Flags == other.Flags
				&& SamplerDesc == other.SamplerDesc
				&& !_stricmp(DebugName.c_str(), other.DebugName.c_str());
		}
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

	bool CreateImage(const RenderContext& context, const ImageDescription& createInfo, AllocatedImage& imageOut);
	uint32_t CalculateMipLevels(uint32_t width, uint32_t height);
	bool TransitionImageLayout(const RenderContext& context, const AllocatedImage& image, EImageLayout oldLayout, EImageLayout newLayout, uint32_t mipLevels);
	bool TransferImage(const RenderContext& context, const ImageDescription& createInfo, const uint8_t** layerArray, uint32_t layerCount, AllocatedImage& imageOut);
	bool GenerateImageMipmaps(const RenderContext& context, AllocatedImage& image, const ImageDescription& imageDesc);
	bool BindDescriptorTexture(const RenderContext& context, cTexture* texture, VkDescriptorSet& set, uint32_t binding, uint32_t arrayIndex);
	bool IsDepthFormat(EFormat format);
	bool IsStencilFormat(EFormat format);
    bool IsUsageCorrect(EImageUsage usage);
    bool LoadTextureFromFile(const RenderContext& context, const char* filepath, cTexture** texture, EFormat format);
	void DestroyCachedTextures(const RenderContext& context);


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

		static cTexture* Create(const RenderContext& context, const ImageDescription& description);
		static void Destroy(const RenderContext& context, cTexture* texture);

		void SetImageLayers(const RenderContext& context, const uint8_t** layerDataArray, uint32_t layerCount);
		void GenerateMipmaps(const RenderContext& context);

		ImageView CreateView(const RenderContext& context, const tViewDescription& viewDesc);
		ImageView GetView(uint32_t viewIndex) const;
		inline VkImage GetNativeImage() const { return m_image.Image; }
		uint32_t GetViewCount() const;
		Sampler GetSampler() const { return m_sampler; }
		void SetSampler(Sampler sampler) { m_sampler = sampler; }
		const ImageDescription& GetDescription() const { return m_description; }
		inline uint32_t GetWidth() const { return m_description.Width; }
		inline uint32_t GetHeight() const { return m_description.Height; }

		inline EImageLayout GetImageLayout() const { return m_layout; }
		// used for render target for update layout after using texture as attachment. Must not be use to change layout tracking.
		inline void SetImageLayout(EImageLayout layout) { m_layout = layout; }
        void TransferImageLayout(const RenderContext& context, EImageLayout dstLayout, VkAccessFlags srcAccess = 0, VkAccessFlags dstAccess = 0,
            VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

		void CmdImageBarrier(VkCommandBuffer cmd, EImageLayout dstLayout, VkAccessFlags srcAccess = 0, VkAccessFlags dstAccess = 0, 
			VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

		inline bool HasDepth() const { return IsDepthFormat(m_description.Format); }
        inline bool HasStencil() const { return IsStencilFormat(m_description.Format); }

	private:
		void Destroy(const RenderContext& context);
		void AllocateImage(const RenderContext& context, const ImageDescription& desc);

		ImageDescription m_description;
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


}