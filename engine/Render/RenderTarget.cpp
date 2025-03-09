#include "Render/RenderTarget.h"
#include "Core/Debug.h"
#include "Render/RenderContext.h"
#include "Render/InitVulkanTypes.h"
#include "Core/Logger.h"
#include "VulkanRenderEngine.h"
#include <type_traits>
#include <imgui.h>

namespace Mist
{
	RenderTargetList g_renderTargets;

	VkAttachmentDescription BuildAttachmentDescription(EFormat format, EImageLayout finalLayout, bool clearOnLoad)
	{
		VkAttachmentDescription desc
		{
			.flags = 0,
			.format = tovk::GetFormat(format),
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = clearOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = clearOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = clearOnLoad ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.finalLayout = tovk::GetImageLayout(finalLayout),
		};
		return desc;
	}

	void RenderTargetAttachment::Destroy(const RenderContext& renderContext)
	{
		// Destroy resources only if there is an image allocated. Otherwise is external framebuffer.
#if 0
		if (Image.IsAllocated())
		{
			vkDestroyImageView(renderContext.Device, View, nullptr);
			MemFreeImage(renderContext.Allocator, Image);
		}
#endif // 0
		if (Tex)
			cTexture::Destroy(renderContext, Tex);
		Tex = nullptr;
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

	void RenderTarget::CreateInternal(const RenderContext& renderContext, const RenderTargetDescription& desc)
	{
		check(ExecuteFormatValidation(renderContext, desc));
		if (!desc.ResourceName.IsEmpty())
			SetName(desc.ResourceName.CStr());
		m_description = desc;
		for (uint32_t i = 0; i < desc.ColorAttachmentDescriptions.GetSize(); ++i)
			m_clearValues.Push(desc.ColorAttachmentDescriptions[i].ClearValue);
		if (desc.DepthAttachmentDescription.IsValidAttachment())
			m_clearValues.Push(desc.DepthAttachmentDescription.ClearValue);
		CreateResources(renderContext);
	}

	void RenderTarget::DestroyInternal(const RenderContext& renderContext)
	{
		check(m_renderPass != VK_NULL_HANDLE);
		check(m_framebuffer != VK_NULL_HANDLE);
		for (uint32_t i = 0; i < (uint32_t)m_description.ColorAttachmentDescriptions.GetSize(); ++i)
		{
			m_attachments[i].Destroy(renderContext);
		}
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			m_attachments[m_description.ColorAttachmentDescriptions.GetSize()].Destroy(renderContext);
		}

		vkDestroyFramebuffer(renderContext.Device, m_framebuffer, nullptr);
		m_framebuffer = VK_NULL_HANDLE;
		vkDestroyRenderPass(renderContext.Device, m_renderPass, nullptr);
		m_renderPass = VK_NULL_HANDLE;
	}

	RenderTarget* RenderTarget::Create(const RenderContext& context, const RenderTargetDescription& desc)
	{
		RenderTarget* rt = _new RenderTarget();
		rt->CreateInternal(context, desc);
		for (uint32_t i = 0; i < (uint32_t)g_renderTargets.size(); ++i)
		{
			if (!g_renderTargets[i])
			{
				g_renderTargets[i] = rt;
				return rt;
			}
		}
		g_renderTargets.push_back(rt);
		return rt;
	}

	void RenderTarget::Destroy(const RenderContext& context, RenderTarget* rt)
	{
		if (rt)
		{
			for (uint32_t i = 0; i < (uint32_t)g_renderTargets.size(); ++i)
			{
				if (g_renderTargets[i] == rt)
					g_renderTargets[i] = nullptr;
			}
			rt->DestroyInternal(context);
			delete rt;
		}
	}

	void RenderTarget::ImGuiRenderTargets()
	{
		ImGui::Begin("RenderTargets");
		size_t size = 0;
		for (uint32_t i = 0; i < (uint32_t)g_renderTargets.size(); ++i)
		{
			const RenderTarget* rt = g_renderTargets[i];
			uint32_t width = rt->GetWidth();
			uint32_t height = rt->GetHeight();
			ImGui::Text("%s ( %4d x %4d )", rt->GetName(), width, height);
			for (uint32_t j = 0; j < rt->GetAttachmentCount(); ++j)
			{
				if (rt->GetTexture(j))
				{
					EFormat format = rt->GetTexture(j)->GetDescription().Format;
					size_t rtSize = (size_t)width * (size_t)height * (size_t)utils::GetPixelSizeFromFormat(format);
					ImGui::BulletText("%s [%s](%6llu Kb)", rt->GetTexture(j)->GetName(), FormatToStr(format), rtSize / 1024l);
					size += rtSize;
				}
				else
					ImGui::BulletText("Not owned texture");
			}
		}
		ImGui::SeparatorText("Stats");
		ImGui::Text("Total RTs: %d", (uint32_t)g_renderTargets.size());
		ImGui::Text("Memory: %6llu Kb", size / 1024l);
		if (ImGui::Button("Dump info"))
			DumpInfo();
		ImGui::End();
	}

	void RenderTarget::DumpInfo()
	{
		loginfo("=================== Render target dump info ==========================\n");
		size_t size = 0;
        for (uint32_t i = 0; i < (uint32_t)g_renderTargets.size(); ++i)
        {
            const RenderTarget* rt = g_renderTargets[i];
            uint32_t width = rt->GetWidth();
            uint32_t height = rt->GetHeight();
			logfinfo("%s ( %4d x %4d )\n", rt->GetName(), width, height);
            for (uint32_t j = 0; j < rt->GetAttachmentCount(); ++j)
            {
                if (rt->GetTexture(j))
                {
                    EFormat format = rt->GetTexture(j)->GetDescription().Format;
                    size_t rtSize = (size_t)width * (size_t)height * (size_t)utils::GetPixelSizeFromFormat(format);
                    logfinfo("\t* %s [%s](%6llu Kb)\n", rt->GetTexture(j)->GetName(), FormatToStr(format), rtSize / 1024l);
                    size += rtSize;
                }
                else
                    loginfo("\t* Not owned texture\n");
            }
        }
        logfinfo("Total RTs: %d\n", (uint32_t)g_renderTargets.size());
        logfinfo("Memory: %6llu Kb\n", size / 1024l);
		loginfo("===================================================\n");
	}

	void RenderTarget::DestroyAll(const RenderContext& context)
	{
		for (uint32_t i = 0; i < (uint32_t)g_renderTargets.size(); ++i)
		{
			if (g_renderTargets[i])
				RenderTarget::Destroy(context, g_renderTargets[i]);
		}
		g_renderTargets.clear();
		g_renderTargets.shrink_to_fit();
	}

	void RenderTarget::Invalidate(const RenderContext& renderContext)
	{
		DestroyInternal(renderContext);
		CreateResources(renderContext);
	}

	void RenderTarget::PreparePass(const RenderContext& context)
	{
		return;
		// transfer image layout to ensure they are in the correct layout
		if (!m_description.ClearOnLoad)
		{
			EImageLayout initialLayout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			uint32_t colorAttachments = m_attachments.GetSize();
			if (HasDepthBufferAttachment())
			{
				--colorAttachments;
				GetDepthAttachment().Tex->TransferImageLayout(context, initialLayout);
			}
			for (uint32_t i = 0; i < colorAttachments; ++i)
				GetAttachment(i).Tex->TransferImageLayout(context, initialLayout);
		}
	}

	void RenderTarget::BeginPass(const RenderContext& context, VkCommandBuffer cmd)
	{
		PreparePass(context);

		VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.renderArea = m_description.RenderArea;
		renderPassInfo.framebuffer = m_framebuffer;
		renderPassInfo.clearValueCount = m_description.ClearOnLoad ? m_clearValues.GetSize() : 0;
		renderPassInfo.pClearValues = m_description.ClearOnLoad ? m_clearValues.GetData() : nullptr;
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void RenderTarget::EndPass(VkCommandBuffer cmd)
	{
		vkCmdEndRenderPass(cmd);

#if 0
		// update attachment image layouts after end pass
		uint32_t colorAttachments = m_attachments.GetSize();
		if (HasDepthBufferAttachment())
		{
			--colorAttachments;
			if (GetDepthAttachment().Tex)
				GetDepthAttachment().Tex->SetImageLayout(m_description.DepthAttachmentDescription.Layout);
		}
		for (uint32_t i = 0; i < colorAttachments; ++i)
		{
			if (GetAttachment(i).Tex)
				GetAttachment(i).Tex->SetImageLayout(m_description.ColorAttachmentDescriptions[i].Layout);
		}
#endif // 0

	}

	void RenderTarget::ClearColor(VkCommandBuffer cmd, float r, float g, float b, float a)
	{
		uint32_t colorAttachments = HasDepthBufferAttachment() ? GetAttachmentCount() - 1 : GetAttachmentCount();
		if (colorAttachments)
		{
			tStaticArray<VkClearAttachment, MAX_RENDER_TARGET_COLOR_ATTACHMENTS> clearInfo;
			tStaticArray<VkClearRect, MAX_RENDER_TARGET_COLOR_ATTACHMENTS> rectInfo;
			for (uint32_t i = 0; i < colorAttachments; ++i)
			{
				VkClearAttachment clearAttachment;
				clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				clearAttachment.clearValue = { .color = {r, g, b, a} };
				clearAttachment.colorAttachment = i;
				clearInfo.Push(clearAttachment);

				VkClearRect r;
				r.rect = m_description.RenderArea;
				r.baseArrayLayer = 0;
				r.layerCount = 1;
				rectInfo.Push(r);
			}
			vkCmdClearAttachments(cmd, colorAttachments, clearInfo.GetData(), colorAttachments, rectInfo.GetData());
		}
	}

	void RenderTarget::ClearDepthStencil(VkCommandBuffer cmd, float depth, uint32_t stencil)
	{
		if (HasDepthBufferAttachment())
		{
			VkClearAttachment clearAttachment;
			clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			clearAttachment.clearValue = { .depthStencil = {depth, stencil} };
			clearAttachment.colorAttachment = UINT32_MAX;

			VkClearRect r;
			r.rect = m_description.RenderArea;
			r.baseArrayLayer = 0;
			r.layerCount = 1;

			vkCmdClearAttachments(cmd, 1, &clearAttachment, 1, &r);
		}
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
		return m_attachments.GetSize();
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
#if 0
		if (m_description.ExternalAttachmentCount == 0)
			CreateFramebuffer(renderContext);
		else
			CreateFramebuffer(renderContext, m_description.ExternalAttachments.data(), m_description.ExternalAttachmentCount);
#else
			CreateFramebuffer(renderContext);
#endif // 0

	}

	void RenderTarget::CreateRenderPass(const RenderContext& renderContext)
	{
		// Create render pass
		check(m_description.ColorAttachmentDescriptions.GetSize() <= MAX_RENDER_TARGET_COLOR_ATTACHMENTS);
		tArray<VkAttachmentDescription, MAX_RENDER_TARGET_ATTACHMENTS> attachments;
		// TODO: do we need to support multiple subpasses inside a RenderPass?
		tArray<VkAttachmentReference, MAX_RENDER_TARGET_ATTACHMENTS> attachmentReferences;
		uint32_t attachmentCount;
#if 0
		if (m_description.ExternalAttachmentCount)
		{
			attachmentCount = m_description.ExternalAttachmentCount;
			for (uint32_t i = 0; i < m_description.ExternalAttachmentCount; ++i)
			{
				// Fill attachment info
				attachments[i] = BuildAttachmentDescription(m_description.ExternalAttachments[i].Format, m_description.ExternalAttachments[i].Layout, m_description.ClearOnLoad);
				// References
				attachmentReferences[i].attachment = i;
				attachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}
		else
#endif // 0

		{
			attachmentCount = m_description.ColorAttachmentDescriptions.GetSize();
			for (uint32_t i = 0; i < attachmentCount; ++i)
			{
				// Fill attachment info
				attachments[i] = BuildAttachmentDescription(m_description.ColorAttachmentDescriptions[i].Format, m_description.ColorAttachmentDescriptions[i].Layout, m_description.ClearOnLoad);
				// References
				attachmentReferences[i].attachment = i;
				attachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}

		// Depth attachment
		VkAttachmentReference* depthReference = VK_NULL_HANDLE;
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			uint32_t depthAttachmentIndex = attachmentCount;
			attachments[depthAttachmentIndex] = BuildAttachmentDescription(m_description.DepthAttachmentDescription.Format, m_description.DepthAttachmentDescription.Layout, m_description.ClearOnLoad);
			attachmentReferences[depthAttachmentIndex].attachment = depthAttachmentIndex;
			attachmentReferences[depthAttachmentIndex].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			++attachmentCount;
			depthReference = &attachmentReferences[depthAttachmentIndex];
		}
		check(attachmentCount > 0);

		VkSubpassDescription subpass;
		subpass.flags = 0;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = m_description.ColorAttachmentDescriptions.GetSize();
		subpass.pColorAttachments = m_description.ColorAttachmentDescriptions.IsEmpty() ? VK_NULL_HANDLE : attachmentReferences.data();
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
		if (!m_description.ColorAttachmentDescriptions.IsEmpty())
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
		sprintf_s(buff, "%s_RenderPass_%d", GetName(), c++);
		SetVkObjectName(renderContext, &m_renderPass, VK_OBJECT_TYPE_RENDER_PASS, buff);
	}

	void RenderTarget::CreateFramebuffer(const RenderContext& renderContext)
	{
		uint32_t colorAttachmentCount = m_description.ColorAttachmentDescriptions.GetSize();
		tStaticArray<VkImageView, MAX_RENDER_TARGET_ATTACHMENTS> views;
		uint32_t attachmentCount = colorAttachmentCount + (m_description.DepthAttachmentDescription.IsValidAttachment() ? 1 : 0);
		m_attachments.Resize(attachmentCount);
		views.Resize(attachmentCount);
		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			CreateAttachment(m_attachments[i], renderContext, m_description.ColorAttachmentDescriptions[i], IMAGE_ASPECT_COLOR_BIT,
				IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
			views[i] = m_attachments[i].View;
		}
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			CreateAttachment(m_attachments[colorAttachmentCount], renderContext, m_description.DepthAttachmentDescription, IMAGE_ASPECT_DEPTH_BIT,
				IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | IMAGE_USAGE_SAMPLED_BIT);
			views[colorAttachmentCount] = m_attachments[colorAttachmentCount].View;
		}
		VkFramebufferCreateInfo fbInfo = vkinit::FramebufferCreateInfo(m_renderPass,
			m_description.RenderArea.extent.width,
			m_description.RenderArea.extent.height,
			views.GetData(), attachmentCount);
		vkcheck(vkCreateFramebuffer(renderContext.Device, &fbInfo, nullptr, &m_framebuffer));
	}

#if 0
	void RenderTarget::CreateFramebuffer(const RenderContext& renderContext, const RenderTargetExternalAttachmentDescription* externalAttachments, uint32_t viewCount)
	{
		check(viewCount && viewCount <= MAX_RENDER_TARGET_ATTACHMENTS);
		tArray<VkImageView, MAX_RENDER_TARGET_ATTACHMENTS> views;
		m_attachments.Resize(viewCount);
		for (uint32_t i = 0; i < viewCount; ++i)
		{
			m_attachments[i].View = externalAttachments[i].View;
#if 0
			m_attachments[i].Image.Alloc = VK_NULL_HANDLE;
			m_attachments[i].Image.Image = VK_NULL_HANDLE;
#endif // 0
			views[i] = externalAttachments[i].View;
		}
		VkFramebufferCreateInfo fbInfo = vkinit::FramebufferCreateInfo(m_renderPass,
			m_description.RenderArea.extent.width,
			m_description.RenderArea.extent.height,
			views.data(), viewCount);
		vkcheck(vkCreateFramebuffer(renderContext.Device, &fbInfo, nullptr, &m_framebuffer));
		tFixedString<32> name;
		name.Fmt("%s_Framebuffer", GetName());
		SetVkObjectName(renderContext, &m_framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER, name.CStr());
	}
#endif // 0


	void RenderTarget::CreateAttachment(RenderTargetAttachment& attachment, const RenderContext& renderContext, const RenderTargetAttachmentDescription& description, EImageAspect aspect, EImageUsage imageUsage) const
	{
		if (!description.Attachment)
		{
			tImageDescription imageDesc;
			imageDesc.Width = m_description.RenderArea.extent.width;
			imageDesc.Height = m_description.RenderArea.extent.height;
			imageDesc.Format = description.Format;
			imageDesc.Usage = imageUsage | description.AdditionalUsage;
			imageDesc.SampleCount = SAMPLE_COUNT_1_BIT;
			imageDesc.DebugName.Fmt("%s_%s_AttachmentTexture_%d", GetName(), IsDepthFormat(description.Format) ? "Depth" : "Color", 0);
			attachment.Tex = cTexture::Create(renderContext, imageDesc);

			if (!m_description.ClearOnLoad)
			{
				attachment.Tex->TransferImageLayout(renderContext, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}

			tViewDescription viewDesc;
			viewDesc.AspectMask = aspect;
			attachment.View = attachment.Tex->CreateView(renderContext, viewDesc);
		}
		else
		{
			attachment.View = description.Attachment;
		}
	}

	bool RenderTarget::ExecuteFormatValidation(const RenderContext& renderContext, const RenderTargetDescription& description)
	{
		bool res = true;
		for (uint32_t i = 0; i < description.ColorAttachmentDescriptions.GetSize(); ++i)
		{
			VkFormatProperties prop;
			vkGetPhysicalDeviceFormatProperties(renderContext.GPUDevice, tovk::GetFormat(description.ColorAttachmentDescriptions[i].Format), &prop);
			if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
			{
				logferror("Color Attachment %d with format %s has an invalid format (not supported by VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)\n", i, FormatToStr(description.ColorAttachmentDescriptions[i].Format));
				res = false;
			}
		}
		if (description.DepthAttachmentDescription.IsValidAttachment())
		{
			VkFormatProperties prop;
			vkGetPhysicalDeviceFormatProperties(renderContext.GPUDevice, tovk::GetFormat(description.DepthAttachmentDescription.Format), &prop);
			if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
			{
				logferror("Depth Attachment with format %s has an invalid format (not supported by VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)\n", FormatToStr(description.DepthAttachmentDescription.Format));
				res = false;
			}
		}
		return res;
	}

	VkImageView RenderTarget::GetDepthBuffer() const
	{
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
		{
			return m_attachments[m_description.ColorAttachmentDescriptions.GetSize()].View;
		}
		return VK_NULL_HANDLE;
	}

	const cTexture* RenderTarget::GetDepthTexture() const
	{
		if (m_description.DepthAttachmentDescription.IsValidAttachment())
			return m_attachments[m_description.ColorAttachmentDescriptions.GetSize()].Tex;
		return nullptr;
	}
}