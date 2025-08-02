#pragma once

#ifdef RBE_VK
#include <vulkan/vulkan.h>
#endif

#include "Types.h"

namespace render
{
    struct TextureSubresourceRange;
    struct TextureSubresourceLayer;
    struct TextureBarrier;
    struct TextureDescription;
    struct BufferDescription;

    namespace utils
    {
        struct ImageLayoutState
        {
            VkImageLayout layout;
            VkAccessFlags2 accessFlags;
            VkPipelineStageFlags2 stageFlags;
        };
        ImageLayoutState ConvertImageLayoutState(ImageLayout layout);
        VkImageMemoryBarrier2 ConvertImageBarrier(const TextureBarrier& barrier);

        VkPresentModeKHR ConvertPresentMode(PresentMode mode);
        VkColorSpaceKHR ConvertColorSpace(ColorSpace space);
        VkQueueFlags ConvertQueueFlags(QueueType flags);
        VkBufferUsageFlags ConvertBufferUsage(BufferUsage usage);
        VkImageUsageFlags ConvertImageUsage(ImageUsage usage);
        VkDescriptorType ConvertToDescriptorType(ResourceType type);
        VkFormat ConvertFormat(Format format);
        uint32_t GetBytesPerPixel(Format format);
		uint32_t GetFormatSize(Format format);
        VkImageLayout ConvertImageLayout(ImageLayout layout);
#if !defined(RBE_MEM_MANAGEMENT)
        VmaMemoryUsage ConvertMemoryUsage(MemoryUsage usage);
#else
        VkMemoryPropertyFlags GetMemPropertyFlags(MemoryUsage usage);
#endif
        VkImageType ConvertImageType(ImageDimension dim);
        VkImageViewType ConvertImageViewType(ImageDimension dim);
        VkImageAspectFlags ConvertImageAspectFlags(Format format);
        bool IsDepthFormat(Format format);
        bool IsStencilFormat(Format format);
        bool IsDepthStencilFormat(Format format);
        VkImageSubresourceRange ConvertImageSubresourceRange(const TextureSubresourceRange& range, Format format);
        VkImageSubresourceLayers ConvertImageSubresourceLayer(const TextureSubresourceLayer& layer, Format format);
        VkFilter ConvertFilter(Filter filter);
        VkSamplerMipmapMode ConvertMipmapMode(Filter filter);
        VkSamplerAddressMode ConvertSamplerAddressMode(SamplerAddressMode mode);
        VkShaderStageFlags ConvertShaderStage(ShaderType type);
        VkBlendFactor ConvertBlendFactor(BlendFactor factor);
        VkBlendOp ConvertBlendOp(BlendOp op);
        VkCompareOp ConvertCompareOp(CompareOp op);
        VkColorComponentFlags ConvertColorComponentFlags(ColorMask mask);
        VkStencilOp ConvertStencilOp(StencilOp op);
        VkPrimitiveTopology ConvertPrimitiveType(PrimitiveType type);
        VkPolygonMode ConvertPolygonMode(RasterFillMode mode);
        VkCullModeFlags ConvertCullMode(RasterCullMode mode);

        void ComputeMipExtent(uint32_t mipLevel, uint32_t width, uint32_t height, uint32_t depth, uint32_t* mipWidth, uint32_t* mipHeight, uint32_t* mipDepth);
        uint32_t ComputeMipLevels(uint32_t width, uint32_t height);

        inline uint64_t Align(uint64_t size, uint64_t alignment) { return (size + alignment - 1) & ~(alignment - 1); }

        template <typename T, typename U>
        inline bool EqualArrays(const T* t, uint32_t ct, const U* u, uint32_t cu)
        {
            if (cu != ct)
                return false;
            for (uint32_t i = 0; i < ct; ++i)
            {
                if (t[i] != u[i])
                    return false;
            }
            return true;
        }

        template <typename T, typename U>
        inline void Swap(T& t, U& u)
        {
            T temp = t;
            t = u;
            u = t;
        }

        BufferUsage GetBufferUsage(const BufferDescription& description);
        ImageUsage GetImageUsage(const TextureDescription& description);
    }
}