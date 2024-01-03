// header file for vkmmc project 

#include "RenderPass.h"
#include "RenderPipeline.h"
#include "RenderTypes.h"
#include "Framebuffer.h"

namespace vkmmc
{
	struct RenderFrameContext;
	class Model;

	struct RendererCreateInfo
	{
		RenderContext RContext;
		VkDescriptorSetLayout GlobalDescriptorSetLayout;


		VkPushConstantRange* ConstantRange = nullptr;
		uint32_t ConstantRangeCount = 0;
	};

	class IRendererBase
	{
	public:
		IRendererBase() = default;
		virtual ~IRendererBase() = default;

		virtual void Init(const RendererCreateInfo& info) = 0;
		virtual void Destroy(const RenderContext& renderContext) = 0;
		virtual void RecordCommandBuffer(const RenderFrameContext& renderFrameContext, const Model* models, uint32_t modelCount) = 0;

	protected:
		RenderPass m_renderPass;
		Framebuffer m_framebuffer;

		// Render State
		VkDescriptorSetLayout m_descriptorSetLayout;
		RenderPipeline m_renderPipeline;
	};
}