// header file for vkmmc project 

#include "RenderPass.h"
#include "RenderPipeline.h"
#include "RenderTypes.h"
#include "Framebuffer.h"

namespace vkmmc
{
	struct RenderFrameContext;

	struct RendererCreateInfo
	{
		RenderContext RContext;
		VkDescriptorSetLayout GlobalDescriptorSet;
		VkPushConstantRange ConstantRange;
	};

	class IRendererBase
	{
	public:
		IRendererBase() = default;
		virtual ~IRendererBase() = default;

		virtual void Init(const RendererCreateInfo& info) = 0;
		virtual void Destroy(const RenderContext& renderContext) = 0;
		virtual void RecordCommandBuffer(const RenderFrameContext& renderFrameContext) = 0;

	protected:
		RenderPass m_renderPass;
		Framebuffer m_framebuffer;

		// Render State
		VkDescriptorSetLayout m_descriptorSetLayout;
		RenderPipeline m_renderPipeline;
	};
}