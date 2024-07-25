// src file for Mist project 
#include "Render/Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#include "Render/Shader.h"
#include "Core/Debug.h"
#include "Core/Logger.h"
#include "Render/RenderTypes.h"
#include "Render/RenderContext.h"
#include "Render/InitVulkanTypes.h"


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

#if 1
	void Texture::Destroy(const RenderContext& context)
	{
		vkDestroySampler(context.Device, m_sampler, nullptr);
		
		for (uint32_t i = 0; i < (uint32_t)m_views.size(); ++i)
			vkDestroyImageView(context.Device, m_views[i], nullptr);
		MemFreeImage(context.Allocator, m_image);
	}

	void Texture::AllocateImage(const RenderContext& context, const tImageDescription& desc)
	{
		check(!m_image.IsAllocated());
		m_description = desc;
		check(CreateImage(context, desc, m_image));

		// Format requirement check
		{
			VkImageFormatProperties imageFormatProperties;
			vkcheck(vkGetPhysicalDeviceImageFormatProperties(context.GPUDevice, tovk::GetFormat(desc.Format),
				VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				0, &imageFormatProperties));
		}

		VkSamplerCreateInfo samplerCreateInfo = vkinit::SamplerCreateInfo(FILTER_LINEAR, SAMPLER_ADDRESS_MODE_REPEAT);
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.minLod = 0.f;
		samplerCreateInfo.maxLod = (float)desc.MipLevels;
		samplerCreateInfo.mipLodBias = 0.f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		vkcheck(vkCreateSampler(context.Device, &samplerCreateInfo, nullptr, &m_sampler));
	}

	Texture* Texture::Create(const RenderContext& context, const tImageDescription& description)
	{
		Texture* texture = new Texture();
		texture->AllocateImage(context, description);
		return texture;
	}

	void Texture::Destroy(const RenderContext& context, Texture* texture)
	{
		texture->Destroy(context);
		delete texture;
	}

	void Texture::SetImageLayers(const RenderContext& context, const uint8_t** layerDataArray, uint32_t layerCount)
	{
		check(layerCount == m_description.Layers);
		check(m_image.IsAllocated());
		check(TransferImage(context, m_description, layerDataArray, layerCount, m_image));
	}

	void Texture::GenerateMipmaps(const RenderContext& context)
	{
		for (uint32_t i = 1; i <= m_description.MipLevels; ++i)
			TransitionImageLayout(context, m_image, IMAGE_LAYOUT_UNDEFINED, IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, i);
		check(GenerateImageMipmaps(context, m_image, m_description));
	}

	ImageView Texture::CreateView(const RenderContext& context, const tViewDescription& viewDesc)
	{
		VkImageViewCreateInfo viewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.image = m_image.Image,
			.viewType = viewDesc.ViewType,
			.format = tovk::GetFormat(m_description.Format),
		};
		check(viewDesc.BaseMipLevel + viewDesc.LevelCount <= m_description.MipLevels);
		check(viewDesc.BaseArrayLayer + viewDesc.LayerCount <= m_description.Layers);
		viewInfo.subresourceRange.baseMipLevel = viewDesc.BaseMipLevel;
		viewInfo.subresourceRange.levelCount = viewDesc.LevelCount;
		viewInfo.subresourceRange.baseArrayLayer = viewDesc.BaseArrayLayer;
		viewInfo.subresourceRange.layerCount = viewDesc.LayerCount;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ImageView view;
		vkcheck(vkCreateImageView(context.Device, &viewInfo, nullptr, &view));
		m_views.push_back(view);

		static int c = 0;
		char buff[256];
		if (c == 90) __debugbreak();
		sprintf_s(buff, "ImageView_%d", c++);
		SetVkObjectName(context, &view, VK_OBJECT_TYPE_IMAGE_VIEW, buff);


		return view;
	}

	ImageView Texture::GetView(uint32_t viewIndex) const
	{
		check(viewIndex < (uint32_t)m_views.size());
		return m_views[viewIndex];
	}

	uint32_t Texture::GetViewCount() const
	{
		return (uint32_t)m_views.size();
	}


#else
	void Texture::Init(const RenderContext& renderContext, const TextureCreateInfo& textureInfo)
	{
		check(!m_image.IsAllocated());
		check(textureInfo.Pixels && textureInfo.Width && textureInfo.Height && textureInfo.Depth);
		//check(textureRaw.Channels == 4 || textureRaw.Channels == 3);

		// Format requirement check
		{
			VkImageFormatProperties imageFormatProperties;
			vkcheck(vkGetPhysicalDeviceImageFormatProperties(renderContext.GPUDevice, tovk::GetFormat(textureInfo.Format),
				VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				0, &imageFormatProperties));
		}

		// Prepare image creation
		tImageDescription desc;
		desc.Width = textureInfo.Width;
		desc.Height = textureInfo.Height;
		desc.Depth = 1;
		desc.Format = textureInfo.Format;
		desc.Layers = 1;
		desc.SampleCount = SAMPLE_COUNT_1_BIT;
		desc.MipLevels = textureInfo.Mipmaps ? CalculateMipLevels(desc.Width, desc.Height) : 1;
		desc.Flags = 0;
		check(CreateImage(renderContext, desc, m_image));
		VkExtent3D extent;
		extent.width = textureInfo.Width;
		extent.height = textureInfo.Height;
		extent.depth = textureInfo.Depth;
#if 0
		VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(textureInfo.Format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, extent);
		m_image = Memory::CreateImage(renderContext.Allocator, imageInfo, MEMORY_USAGE_GPU);
#endif // 0


		// Create transit buffer
		VkDeviceSize size = utils::GetPixelSizeFromFormat(textureInfo.Format) * (VkDeviceSize)textureInfo.PixelCount;
		//check(size == textureInfo.PixelCount);
		AllocatedBuffer stageBuffer = Memory::CreateBuffer(renderContext.Allocator, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEMORY_USAGE_CPU);
		Memory::MemCopy(renderContext.Allocator, stageBuffer, textureInfo.Pixels, size);
		Logf(LogLevel::Info, "Created texture %s size %u bytes.\n", FormatToStr(textureInfo.Format), size);

		utils::CmdSubmitTransfer(renderContext,
			[=](VkCommandBuffer cmd)
			{
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

				// Prepare image to be transfered
				vkCmdPipelineBarrier(cmd,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &transferBarrier);
				// Copy from temporal stage buffer to image
				vkCmdCopyBufferToImage(cmd, stageBuffer.Buffer, m_image.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &copyRegion);

				if (!textureInfo.Mipmaps)
				{
					// Restore image layout to be used.
					VkImageMemoryBarrier imageBarrier = transferBarrier;
					imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					vkCmdPipelineBarrier(cmd,
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
				}
			});

		// Destroy transfer buffer.
		Memory::DestroyBuffer(renderContext.Allocator, stageBuffer);

		if (textureInfo.Mipmaps)
		{
			//TransitionImageLayout(renderContext, m_image, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);
			for (uint32_t i = 1; i <= desc.MipLevels; ++i)
				TransitionImageLayout(renderContext, m_image, IMAGE_LAYOUT_UNDEFINED, IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, i);
			GenerateImageMipmaps(renderContext, m_image, desc);
		}

		static int c = 0;
		char buff[32];
		sprintf_s(buff, "Image_%d", c++);
		SetVkObjectName(renderContext, &m_image.Image, VK_OBJECT_TYPE_IMAGE, buff);

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
		viewInfo.subresourceRange.levelCount = desc.MipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		vkcheck(vkCreateImageView(renderContext.Device, &viewInfo, nullptr, &m_imageView));

		// Sampler
		//m_sampler = CreateSampler(renderContext);
		VkSamplerCreateInfo samplerCreateInfo = vkinit::SamplerCreateInfo(FILTER_LINEAR, SAMPLER_ADDRESS_MODE_REPEAT);
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.minLod = 0.f;
		samplerCreateInfo.maxLod = (float)desc.MipLevels;
		samplerCreateInfo.mipLodBias = 0.f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		vkcheck(vkCreateSampler(renderContext.Device, &samplerCreateInfo, nullptr, &m_sampler));
	}

	void Texture::Destroy(const RenderContext& renderContext)
	{
		Memory::DestroyImage(renderContext.Allocator, m_image);
		vkDestroyImageView(renderContext.Device, m_imageView, nullptr);
		vkDestroySampler(renderContext.Device, m_sampler, nullptr);
	}

	void Texture::Bind(const RenderContext& renderContext, VkDescriptorSet set, VkSampler sampler, uint32_t binding, uint32_t arrayIndex) const
	{
		//check(set != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE);
		VkDescriptorImageInfo imageInfo
		{
			.sampler = m_sampler,
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
#endif // 0


	bool CreateImage(const RenderContext& context, const tImageDescription& createInfo, AllocatedImage& imageOut)
	{
		check(!imageOut.IsAllocated());
		// Prepare image creation
		VkExtent3D extent;
		extent.width = createInfo.Width;
		extent.height = createInfo.Height;
		extent.depth = createInfo.Depth;
		VkImageCreateInfo info;
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = tovk::GetFormat(createInfo.Format);
		info.extent = extent;
		info.mipLevels = createInfo.MipLevels;
		info.arrayLayers = createInfo.Layers;
		info.samples = tovk::GetSampleCount(createInfo.SampleCount);
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = tovk::GetImageUsage(IMAGE_USAGE_TRANSFER_SRC_BIT | IMAGE_USAGE_TRANSFER_DST_BIT | IMAGE_USAGE_SAMPLED_BIT);
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.flags = createInfo.Flags;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageOut = MemNewImage(context.Allocator, info, MEMORY_USAGE_GPU);
		return imageOut.IsAllocated();
	}

	uint32_t CalculateMipLevels(uint32_t width, uint32_t height)
	{
		return (uint32_t)floorf(log2f((float)__max(width, height))) + 1;
	}

	bool TransferImage(const RenderContext& context, const tImageDescription& imageDesc, const uint8_t** layerArray, uint32_t layerCount, AllocatedImage& imageOut)
	{
		// Create transit buffer
		VkDeviceSize size = utils::GetPixelSizeFromFormat(imageDesc.Format) * (VkDeviceSize)(imageDesc.Width * imageDesc.Height * imageDesc.Depth);
		//check(size == textureInfo.PixelCount);
		AllocatedBuffer stageBuffer = MemNewBuffer(context.Allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEMORY_USAGE_CPU);
		//Memory::MemCopy(context.Allocator, stageBuffer, data, size);
		tExtent3D extent;
		extent.width = imageDesc.Width;
		extent.height = imageDesc.Height;
		extent.depth = imageDesc.Depth;

		for (uint32_t i = 0; i < layerCount; ++i)
		{
			Memory::MemCopy(context.Allocator, stageBuffer, layerArray[i], size);

			utils::CmdSubmitTransfer(context,
				[&](CommandBuffer cmd)
				{
					VkBufferImageCopy copyRegion
					{
						.bufferOffset = 0,
						.bufferRowLength = 0,
						.bufferImageHeight = 0
					};
					copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copyRegion.imageSubresource.mipLevel = 0;
					copyRegion.imageSubresource.baseArrayLayer = i;
					copyRegion.imageSubresource.layerCount = 1;
					copyRegion.imageExtent = extent;
					VkImageSubresourceRange range{};
					range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					range.baseMipLevel = 0;
					range.levelCount = 1;
					range.baseArrayLayer = i;
					range.layerCount = 1;
					VkImageMemoryBarrier transferBarrier
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.pNext = nullptr,
						.srcAccessMask = 0,
						.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.image = imageOut.Image,
						.subresourceRange = range,
					};

					// Prepare image to be transfered
					vkCmdPipelineBarrier(cmd,
						VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						0, 0, nullptr, 0, nullptr, 1, &transferBarrier);
					// Copy from temporal stage buffer to image
					vkCmdCopyBufferToImage(cmd, stageBuffer.Buffer, imageOut.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1, &copyRegion);

					if (imageDesc.MipLevels == 1)
					{
						// Restore image layout to be used.
						VkImageMemoryBarrier imageBarrier = transferBarrier;
						imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						vkCmdPipelineBarrier(cmd,
							VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
					}
				});
		}

		MemFreeBuffer(context.Allocator, stageBuffer);

		return true;
	}

	bool TransitionImageLayout(const RenderContext& context, const AllocatedImage& image, EImageLayout oldLayout, EImageLayout newLayout, uint32_t mipLevels)
	{
		utils::CmdSubmitTransfer(context,
			[&](CommandBuffer cmd)
			{
				VkImageMemoryBarrier barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .pNext = nullptr };
				barrier.oldLayout = tovk::GetImageLayout(oldLayout);
				barrier.newLayout = tovk::GetImageLayout(newLayout);
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = image.Image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.subresourceRange.levelCount = mipLevels;

				VkPipelineStageFlags srcStage;
				VkPipelineStageFlags dstStage;
				if (oldLayout == IMAGE_LAYOUT_UNDEFINED && newLayout == IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
				{
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

					srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				}
				else if (oldLayout == IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				{
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
				else
					check(false && "Unsupported transfer operation.");

				vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			});
		return true;
	}

	bool GenerateImageMipmaps(const RenderContext& context, AllocatedImage& image, const tImageDescription& imageDesc)
	{
		// Check if image format supports linear filtering.
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(context.GPUDevice, tovk::GetFormat(imageDesc.Format), &props);
		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			check(false && "Unsupported format to generate mipmap levels.");

		utils::CmdSubmitTransfer(context,
			[&](CommandBuffer cmd)
			{
				VkImageMemoryBarrier barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .pNext = nullptr };
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = image.Image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.subresourceRange.levelCount = 1;

				int32_t width = (int32_t)imageDesc.Width;
				int32_t height = (int32_t)imageDesc.Height;
				for (uint32_t i = 1; i < imageDesc.MipLevels; ++i)
				{
					// Wait to last mip level is in the correct layout
					barrier.subresourceRange.baseMipLevel = i - 1;
					barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

					// Generate current mip level
					VkImageBlit blit;
					blit.srcOffsets[0] = { 0, 0, 0 };
					blit.srcOffsets[1] = { width, height, 1 };
					blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.srcSubresource.mipLevel = i - 1;
					blit.srcSubresource.baseArrayLayer = 0;
					blit.srcSubresource.layerCount = 1;
					blit.dstOffsets[0] = { 0, 0, 0 };
					blit.dstOffsets[1] = { width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1 };
					blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.dstSubresource.mipLevel = i;
					blit.dstSubresource.baseArrayLayer = 0;
					blit.dstSubresource.layerCount = 1;
					vkCmdBlitImage(cmd, image.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

					// Update dimensions
					width = width > 1 ? width / 2 : 1;
					height = height > 1 ? height / 2 : 1;

					// Wait to last miplevel to be ready
					barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				}
				// Wait to last miplevel to be ready
				barrier.subresourceRange.baseMipLevel = imageDesc.MipLevels - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			});
		return true;
	}

	bool BindDescriptorTexture(const RenderContext& context, Texture* texture, VkDescriptorSet& set, uint32_t binding, uint32_t arrayIndex)
	{
		//check(set != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE);
		tViewDescription viewDesc;
		VkDescriptorImageInfo imageInfo
		{
			.sampler = texture->GetSampler(),
			.imageView = texture->GetViewCount() ? texture->GetView(0) : texture->CreateView(context, viewDesc),
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
		vkUpdateDescriptorSets(context.Device, 1, &writeSet, 0, nullptr);
		return true;
	}

	bool LoadTextureFromFile(const RenderContext& context, const char* filepath, Texture** texture, EFormat format)
	{
		check(texture);
		// Load texture from file
		io::TextureRaw texData;
		if (!io::LoadTexture(filepath, texData))
		{
			Logf(LogLevel::Error, "Failed to load texture from %s.\n", filepath);
			return false;
		}

		// Create gpu buffer with texture specifications
		tImageDescription texInfo;
		texInfo.Width = texData.Width;
		texInfo.Height = texData.Height;
		texInfo.Depth = 1;
		//texInfo.Format = utils::GetImageFormatFromChannels(texData.Channels);
		texInfo.Format = format;
#if 1
		texInfo.Flags = 0;
		texInfo.Layers = 1;
		texInfo.MipLevels = CalculateMipLevels(texInfo.Width, texInfo.Height);
		texInfo.SampleCount = SAMPLE_COUNT_1_BIT;
		const uint8_t* pixels = (uint8_t*)texData.Pixels;

		*texture = Texture::Create(context, texInfo);
		(*texture)->SetImageLayers(context, &pixels, 1);
		(*texture)->GenerateMipmaps(context);
#else
		texInfo.Pixels = texData.Pixels;
		texInfo.PixelCount = texData.Width * texData.Height * /*texData.Channels*/1; // depth
		texture.Init(context, texInfo);
#endif // 0


		// Free raw texture data
		io::FreeTexture(texData.Pixels);
		return true;
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
		createInfo.anisotropyEnable = 0;
		Sampler sampler;
		vkcheck(vkCreateSampler(renderContext.Device, &createInfo, nullptr, &sampler));
		g_Samplers[descId] = sampler;
		return sampler;
	}

	Sampler CreateSampler(const RenderContext& renderContext, EFilterType minFilter, EFilterType magFilter, ESamplerAddressMode modeU, ESamplerAddressMode modeV, ESamplerAddressMode modeW)
	{
		return CreateSampler(renderContext, { minFilter, magFilter, modeU, modeV, modeW });
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
