// src file for vkmmc project 
#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#include "RenderPipeline.h"
#include "Debug.h"
#include "RenderTypes.h"

namespace vkutils
{
	vkmmc::EFormat GetImageFormatFromChannels(uint32_t channels)
	{
		switch (channels)
		{
		case 3: return vkmmc::FORMAT_R8G8B8;
		case 4: return vkmmc::FORMAT_R8G8B8A8;
		}
		vkmmc::Logf(vkmmc::LogLevel::Error, "Unsupported number of channels #%d.\n", channels);
		return vkmmc::FORMAT_INVALID;
	}
}

namespace vkmmc
{

	bool io::LoadTexture(const char* path, TextureRaw& out)
	{
		if (!path || !*path)
			return false;

		//stbi_set_flip_vertically_on_load(true);
		int32_t width, height, channels;
		stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels)
		{
			Logf(LogLevel::Error, "Fail to load texture data from %s.\n", path);
			return false;
		}
		if (channels != 4)
		{
			channels = 4;
		}

		out.Channels = (uint32_t)channels;
		out.Width = (uint32_t)width;
		out.Height = (uint32_t)height;
		out.Pixels = pixels;
		return true;
	}

	void io::FreeTexture(unsigned char* data)
	{
		stbi_image_free(data);
	}

	void Texture::Init(const RenderTextureCreateInfo& info)
	{
		check(!m_image.IsAllocated());
		check(info.Raw.Pixels && info.Raw.Width && info.Raw.Height);
		check(info.Raw.Channels == 4 || info.Raw.Channels == 3);

		// Create transit buffer
		VkDeviceSize size = info.Raw.Width * info.Raw.Height * info.Raw.Channels;
		EFormat imageFormat = vkutils::GetImageFormatFromChannels(info.Raw.Channels);
		VkFormat format = types::FormatType(imageFormat);
		AllocatedBuffer stageBuffer = Memory::CreateBuffer(info.RContext.Allocator, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		Memory::MemCopyDataToBuffer(info.RContext.Allocator, stageBuffer.Alloc, info.Raw.Pixels, size);

		// Prepare image creation
		VkExtent3D extent;
		extent.width = info.Raw.Width;
			extent.height = info.Raw.Height;
			extent.depth = 1;
		m_image = Memory::CreateImage(info.RContext.Allocator,
			format, extent,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		utils::CmdSubmitTransfer(info.RContext, 
			[=](VkCommandBuffer cmd) 
			{
				VkImageSubresourceRange range{};
				range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				range.baseMipLevel = 0;
				range.levelCount = 1;
				range.baseArrayLayer = 0;
				range.layerCount = 1;
				VkImageMemoryBarrier transferBarrier
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.image = m_image.Image,
					.subresourceRange = range,
				};
				vkCmdPipelineBarrier(cmd,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &transferBarrier);
				VkBufferImageCopy copyRegion
				{
					.bufferOffset = 0,
					.bufferRowLength = 0,
					.bufferImageHeight = 0
				};
				copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.imageSubresource.mipLevel = 0;
				copyRegion.imageSubresource.baseArrayLayer = 0;
				copyRegion.imageSubresource.layerCount = 1;
				copyRegion.imageExtent = extent;
				// Copy buffer
				vkCmdCopyBufferToImage(cmd, stageBuffer.Buffer, m_image.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &copyRegion);
				VkImageMemoryBarrier imageBarrier = transferBarrier;
				imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
			});

		// Destroy transfer buffer.
		Memory::DestroyBuffer(info.RContext.Allocator, stageBuffer);

		// Create Image view
		VkImageViewCreateInfo viewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.image = m_image.Image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
		};
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		vkcheck(vkCreateImageView(info.RContext.Device, &viewInfo, nullptr, &m_imageView));
	}

	void Texture::Destroy(const RenderContext& renderContext)
	{
		Memory::DestroyImage(renderContext.Allocator, m_image);
		vkDestroyImageView(renderContext.Device, m_imageView, nullptr);
	}
}
