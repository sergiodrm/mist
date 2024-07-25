#include "Render/RenderTarget.h"
#include "Core/Debug.h"
#include "Render/RenderContext.h"
#include "Render/InitVulkanTypes.h"
#include "Core/Logger.h"

namespace Mist
{
	VkAttachmentDescription GetVkDescription(const RenderTargetAttachmentDescription& desc)
	{
		return vkinit::RenderPassAttachmentDescription(desc.Format, desc.Layout);
	}

	void RenderTargetAttachment::Destroy(const RenderContext& renderContext)
	{
		// Destroy resources only if there is an image allocated. Otherwise is external framebuffer.
		if (Image.IsAllocated())
		{
			vkDestroyImageView(renderContext.Device, View, nullptr);
			MemFreeImage(renderContext.Allocator, Image);
		}
	}

	RenderTarget::RenderTarget()
		:
#ifdef MIST_VULKAN
		m_renderPass(VK_NULL_HANDLE),
		m_framebuffer(VK_NULL_HANDLE)
#endif // MIST_VULKAN
	{
	}

	RenderTarget::~RenderTarget()
	{
		check(m_renderPass == VK_NULL_HANDLE);
		check(m_framebuffer == VK_NULL_HANDLE);
	}

	void RenderTarget::Create(const RenderContext& renderContext, const RenderTargetDescription& desc)
	{
		check(ExecuteFormatValidation(renderContext, desc));
		m_description = desc;
		for (uint32_t i = 0; i < desc.ColorAttachmentCount; ++i)
			m_clearValues[i] = desc.ColorAttachmentDescriptions[i].ClearValue;
		if (desc.DepthAttachmentDescription.IsValidAttachment())
			m_clearValues[desc.ColorAttachmentCount] = desc.DepthAttachmentDescription.ClearValue;
		CreateResources(renderContext);
	}

	void RenderTarget::Destroy(const RenderContext& renderContext)
	{
		check(m_renderPass != VK_NULL_HANDLE);
		check(m_framebuffer != VK_NULL_HANDLE);
		for (uint32_t i = 0; i < (uint32_t)m_description.ColorAttachmentCount; ++i)
		{
			m_attachments[i].Destroy(renderContext);
		}
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			m_attachments[m_description.ColorAttachmentCount].Destroy(renderContext);
		}

		vkDestroyFramebuffer(renderContext.Device, m_framebuffer, nullptr);
		m_framebuffer = VK_NULL_HANDLE;
		vkDestroyRenderPass(renderContext.Device, m_renderPass, nullptr);
		m_renderPass = VK_NULL_HANDLE;
	}

	void RenderTarget::Invalidate(const RenderContext& renderContext)
	{
		Destroy(renderContext);
		CreateResources(renderContext);
	}

	void RenderTarget::BeginPass(VkCommandBuffer cmd)
	{
		VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.renderArea = m_description.RenderArea;
		renderPassInfo.framebuffer = m_framebuffer;
		renderPassInfo.clearValueCount = (uint32_t)m_clearValues.size();
		renderPassInfo.pClearValues = m_clearValues.data();
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void RenderTarget::EndPass(VkCommandBuffer cmd)
	{
		vkCmdEndRenderPass(cmd);
	}

	uint32_t RenderTarget::GetWidth() const
	{
		return m_description.RenderArea.extent.width;
	}

	uint32_t RenderTarget::GetHeight() const
	{
		return m_description.RenderArea.extent.height;
	}

	uint32_t RenderTarget::GetAttachmentCount() const
	{
		return (uint32_t)m_attachments.size();
	}

	const RenderTargetAttachment& RenderTarget::GetAttachment(uint32_t index) const
	{
		check(index < GetAttachmentCount());
		return m_attachments[index];
	}

	const RenderTargetDescription& RenderTarget::GetDescription() const
	{
		return m_description;
	}

	void RenderTarget::CreateResources(const RenderContext& renderContext)
	{
		check(m_renderPass == VK_NULL_HANDLE);
		check(m_framebuffer == VK_NULL_HANDLE);
		CreateRenderPass(renderContext);
		if (m_description.ExternalAttachments.at(0) == VK_NULL_HANDLE)
			CreateFramebuffer(renderContext);
		else
			CreateFramebuffer(renderContext, m_description.ExternalAttachments.data(), m_description.ExternalAttachmentCount);
	}

	void RenderTarget::CreateRenderPass(const RenderContext& renderContext)
	{
		// Create render pass
		check(m_description.ColorAttachmentCount <= MAX_RENDER_TARGET_COLOR_ATTACHMENTS);
		tArray<VkAttachmentDescription, MAX_RENDER_TARGET_ATTACHMENTS> attachments;
		// TODO: do we need to support multiple subpasses inside a RenderPass?
		tArray<VkAttachmentReference, MAX_RENDER_TARGET_ATTACHMENTS> attachmentReferences;
		for (uint32_t i = 0; i < m_description.ColorAttachmentCount;++i)
		{
			// Fill attachment info
			attachments[i] = GetVkDescription(m_description.ColorAttachmentDescriptions[i]);

			// References
			attachmentReferences[i].attachment = i;
			attachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		// Depth attachment
		uint32_t attachmentCount = m_description.ColorAttachmentCount;
		VkAttachmentReference* depthReference = VK_NULL_HANDLE;
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			uint32_t depthAttachmentIndex = m_description.ColorAttachmentCount;
			attachments[depthAttachmentIndex] = GetVkDescription(m_description.DepthAttachmentDescription);
			attachmentReferences[depthAttachmentIndex].attachment = depthAttachmentIndex;
			attachmentReferences[depthAttachmentIndex].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			++attachmentCount;
			depthReference = &attachmentReferences[depthAttachmentIndex];
		}

		VkSubpassDescription subpass;
		subpass.flags = 0;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = m_description.ColorAttachmentCount;
		subpass.pColorAttachments = m_description.ColorAttachmentCount ? attachmentReferences.data() : VK_NULL_HANDLE;
		subpass.pDepthStencilAttachment = depthReference;
		// TODO: implement more type of attachments (input?, preserve and resolve)
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;
		subpass.pResolveAttachments = nullptr;

		// Create dependencies
#if 1
		tArray<VkSubpassDependency, 4> dependencies;
		uint32_t dependencyCount = 0;
		if (m_description.ColorAttachmentCount)
		{
			{
				VkSubpassDependency& depedency = dependencies[dependencyCount++];
				depedency.srcSubpass = VK_SUBPASS_EXTERNAL;
				depedency.dstSubpass = 0;
				depedency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				depedency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}
			{
				VkSubpassDependency& depedency = dependencies[dependencyCount++];
				depedency.srcSubpass = 0;
				depedency.dstSubpass = VK_SUBPASS_EXTERNAL;
				depedency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				depedency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}
		}

		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			{
				VkSubpassDependency& depedency = dependencies[dependencyCount++];
				depedency.srcSubpass = VK_SUBPASS_EXTERNAL;
				depedency.dstSubpass = 0;
				depedency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				depedency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}

			{
				VkSubpassDependency& depedency = dependencies[dependencyCount++];
				depedency.srcSubpass = 0;
				depedency.dstSubpass = VK_SUBPASS_EXTERNAL;
				depedency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				depedency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				depedency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				depedency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				depedency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}
		}
#else
		std::array<VkSubpassDependency, 2> dependencies;
		uint32_t dependencyCount = (uint32_t)dependencies.size();
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
#endif // 0


		VkRenderPassCreateInfo passCreateInfo{ .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .pNext = nullptr };
		passCreateInfo.flags = 0;
		passCreateInfo.attachmentCount = attachmentCount;
		passCreateInfo.pAttachments = attachments.data();
		passCreateInfo.subpassCount = 1;
		passCreateInfo.pSubpasses = &subpass;
		passCreateInfo.dependencyCount = dependencyCount;
		passCreateInfo.pDependencies = dependencies.data();

		vkcheck(vkCreateRenderPass(renderContext.Device, &passCreateInfo, nullptr, &m_renderPass));
		static int c = 0;
		char buff[64];
		sprintf_s(buff, "RT_RenderPass_%d", c++);
		SetVkObjectName(renderContext, &m_renderPass, VK_OBJECT_TYPE_RENDER_PASS, buff);
	}

	void RenderTarget::CreateFramebuffer(const RenderContext& renderContext)
	{
		uint32_t viewCount = 0;
		tArray<VkImageView, MAX_RENDER_TARGET_ATTACHMENTS> views;
		for (uint32_t i = 0; i < m_description.ColorAttachmentCount; ++i)
		{
			CreateAttachment(m_attachments[i], renderContext, m_description.ColorAttachmentDescriptions[i], IMAGE_ASPECT_COLOR_BIT, 
				IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
			views[i] = m_attachments[i].View;
			++viewCount;
		}
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			CreateAttachment(m_attachments[m_description.ColorAttachmentCount], renderContext, m_description.DepthAttachmentDescription, IMAGE_ASPECT_DEPTH_BIT,
				IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
			views[m_description.ColorAttachmentCount] = m_attachments[m_description.ColorAttachmentCount].View;
			++viewCount;
		}
		VkFramebufferCreateInfo fbInfo = vkinit::FramebufferCreateInfo(m_renderPass, 
			m_description.RenderArea.extent.width, 
			m_description.RenderArea.extent.height,
			views.data(), viewCount);
		vkcheck(vkCreateFramebuffer(renderContext.Device, &fbInfo, nullptr, &m_framebuffer));
	}

	void RenderTarget::CreateFramebuffer(const RenderContext& renderContext, const VkImageView* views, uint32_t viewCount)
	{
		check(viewCount && viewCount <= MAX_RENDER_TARGET_ATTACHMENTS);
		for (uint32_t i = 0; i < viewCount; ++i)
		{
			m_attachments[i].View = views[i];
			m_attachments[i].Image.Alloc = VK_NULL_HANDLE;
			m_attachments[i].Image.Image = VK_NULL_HANDLE;
		}
		VkFramebufferCreateInfo fbInfo = vkinit::FramebufferCreateInfo(m_renderPass,
			m_description.RenderArea.extent.width,
			m_description.RenderArea.extent.height,
			views, viewCount);
		vkcheck(vkCreateFramebuffer(renderContext.Device, &fbInfo, nullptr, &m_framebuffer));
	}

	void RenderTarget::CreateAttachment(RenderTargetAttachment& attachment, const RenderContext& renderContext, const RenderTargetAttachmentDescription& description, EImageAspect aspect, EImageUsage imageUsage) const
	{

		tExtent3D extent{ .width = m_description.RenderArea.extent.width, .height = m_description.RenderArea.extent.height, .depth = 1 };
		VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(description.Format, imageUsage, extent);
		attachment.Image = MemNewImage(renderContext.Allocator, imageInfo, MEMORY_USAGE_GPU);
		static int c = 0;
		char buff[64];
		sprintf_s(buff, "RT_Image_%d", c);
		SetVkObjectName(renderContext, &attachment.Image.Image, VK_OBJECT_TYPE_IMAGE, buff);
		Logf(LogLevel::Debug, "Render target: %s...\n", buff);

		VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(description.Format, attachment.Image.Image, aspect);
		vkcheck(vkCreateImageView(renderContext.Device, &viewInfo, nullptr, &attachment.View));
		sprintf_s(buff, "RT_ImageView_%d", c++);
		SetVkObjectName(renderContext, &attachment.View, VK_OBJECT_TYPE_IMAGE_VIEW, buff);
	}

	bool RenderTarget::ExecuteFormatValidation(const RenderContext& renderContext, const RenderTargetDescription& description) 
	{
		bool res = true;
		for (uint32_t i = 0; i < description.ColorAttachmentCount; ++i)
		{
			VkFormatProperties prop;
			vkGetPhysicalDeviceFormatProperties(renderContext.GPUDevice, tovk::GetFormat(description.ColorAttachmentDescriptions[i].Format), &prop);
			if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
			{
				Logf(LogLevel::Error, "Color Attachment %d with format %s has an invalid format (not supported by VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)\n", i, FormatToStr(description.ColorAttachmentDescriptions[i].Format));
				res = false;
			}
		}
		if (description.DepthAttachmentDescription.IsValidAttachment())
		{
			VkFormatProperties prop;
			vkGetPhysicalDeviceFormatProperties(renderContext.GPUDevice, tovk::GetFormat(description.DepthAttachmentDescription.Format), &prop);
			if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
			{
				Logf(LogLevel::Error, "Depth Attachment with format %s has an invalid format (not supported by VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)\n", FormatToStr(description.DepthAttachmentDescription.Format));
				res = false;
			}
		}
		return res;
	}


	VkImageView RenderTarget::GetDepthBuffer() const
	{
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			return m_attachments[m_description.ColorAttachmentCount].View;
		}
		return VK_NULL_HANDLE;
	}
}