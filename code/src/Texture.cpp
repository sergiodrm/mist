// src file for Mist project 
#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#include "Shader.h"
#include "Debug.h"
#include "Logger.h"
#include "RenderTypes.h"
#include "RenderContext.h"
#include "InitVulkanTypes.h"


namespace Mist
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

	void Texture::Init(const RenderContext& renderContext, const TextureCreateInfo& textureInfo)
	{
		check(!m_image.IsAllocated());
		check(textureInfo.Pixels && textureInfo.Width && textureInfo.Height && textureInfo.Depth);
		//check(textureRaw.Channels == 4 || textureRaw.Channels == 3);
		VkImageFormatProperties imageFormatProperties;
		vkcheck(vkGetPhysicalDeviceImageFormatProperties(renderContext.GPUDevice, tovk::GetFormat(textureInfo.Format),
			VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			0, &imageFormatProperties));

		// Create transit buffer
		VkDeviceSize size = utils::GetPixelSizeFromFormat(textureInfo.Format) * (VkDeviceSize)textureInfo.PixelCount;
		//check(size == textureInfo.PixelCount);
		AllocatedBuffer stageBuffer = Memory::CreateBuffer(renderContext.Allocator, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEMORY_USAGE_CPU);
		Memory::MemCopy(renderContext.Allocator, stageBuffer, textureInfo.Pixels, size);

		// Prepare image creation
		VkExtent3D extent;
		extent.width = textureInfo.Width;
		extent.height = textureInfo.Height;
		extent.depth = textureInfo.Depth;
		VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(textureInfo.Format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);
		m_image = Memory::CreateImage(renderContext.Allocator, imageInfo, MEMORY_USAGE_GPU);

		utils::CmdSubmitTransfer(renderContext, 
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
		Memory::DestroyBuffer(renderContext.Allocator, stageBuffer);

		// Create Image view
		VkImageViewCreateInfo viewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.image = m_image.Image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = tovk::GetFormat(textureInfo.Format),
		};
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		vkcheck(vkCreateImageView(renderContext.Device, &viewInfo, nullptr, &m_imageView));
	}

	void Texture::Destroy(const RenderContext& renderContext)
	{
		Memory::DestroyImage(renderContext.Allocator, m_image);
		vkDestroyImageView(renderContext.Device, m_imageView, nullptr);
	}

	void Texture::Bind(const RenderContext& renderContext, VkDescriptorSet set, VkSampler sampler, uint32_t binding, uint32_t arrayIndex) const
	{
		check(set != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE);
		VkDescriptorImageInfo imageInfo
		{
			.sampler = sampler,
			.imageView = m_imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkWriteDescriptorSet writeSet
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = set,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &imageInfo
		};
		vkUpdateDescriptorSets(renderContext.Device, 1, &writeSet, 0, nullptr);
	}


	tMap<uint32_t, Sampler> g_Samplers;

	Sampler CreateSampler(const RenderContext& renderContext, const SamplerDescription& description)
	{
		uint32_t descId = GetSamplerPackedDescription(description);
		// Already created?
		if (g_Samplers.contains(descId))
		{
			check(g_Samplers[descId] != VK_NULL_HANDLE);
			return g_Samplers[descId];
		}

		// Create and stored
		VkSamplerCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
		createInfo.addressModeU = tovk::GetSamplerAddressMode(description.AddressModeU);
		createInfo.addressModeV = tovk::GetSamplerAddressMode(description.AddressModeV);
		createInfo.addressModeW = tovk::GetSamplerAddressMode(description.AddressModeW);
		createInfo.minFilter = tovk::GetFilter(description.MinFilter);
		createInfo.magFilter = tovk::GetFilter(description.MagFilter);
		Sampler sampler;
		vkcheck(vkCreateSampler(renderContext.Device, &createInfo, nullptr, &sampler));
		g_Samplers[descId] = sampler;
		return sampler;
	}

	Sampler CreateSampler(const RenderContext& renderContext, EFilterType minFilter, EFilterType magFilter, ESamplerAddressMode modeU, ESamplerAddressMode modeV, ESamplerAddressMode modeW)
	{
		return CreateSampler(renderContext, {minFilter, magFilter, modeU, modeV, modeW});
	}

	uint32_t GetSamplerPackedDescription(const SamplerDescription& desc)
	{
		uint32_t res = (1 << desc.MinFilter) 
			| (1 << (desc.MagFilter + 3))
			| (1 << (desc.AddressModeU + 6))
			| (1 << (desc.AddressModeV + 11))
			| (1 << (desc.AddressModeW + 16));
		return res;
	}

	void DestroySamplers(const RenderContext& renderContext)
	{
		for (tMap<uint32_t, Sampler>::iterator it = g_Samplers.begin();
			it != g_Samplers.end(); ++it)
		{
			check(it->second != VK_NULL_HANDLE);
			vkDestroySampler(renderContext.Device, it->second, nullptr);
		}
		g_Samplers.clear();
	}
}
