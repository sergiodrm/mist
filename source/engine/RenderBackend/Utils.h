#pragma once

#ifdef RBE_VK
#include <vulkan/vulkan.h>
#endif

#include "Types.h"

namespace render
{
    struct TextureSubresourceRange;

    namespace utils
    {
        VkBufferUsageFlags ConvertBufferUsage(BufferUsage usage);
        VkImageUsageFlags ConvertImageUsage(ImageUsage usage);
        VkFormat ConvertFormat(Format format);
        uint32_t GetBytesPerPixel(Format format);
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
        VkFilter ConvertFilter(Filter filter);
        VkSamplerAddressMode ConvertSamplerAddressMode(SamplerAddressMode mode);
        VkShaderStageFlags ConvertShaderStage(ShaderType type);
    }
}