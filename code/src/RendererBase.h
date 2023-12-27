// header file for vkmmc project 

#include "RenderPass.h"

namespace vkmmc
{
	struct RenderContext;
	struct RenderFrameContext;

	class IRendererBase
	{
	public:
		IRendererBase();
		virtual ~IRendererBase() = default;

		virtual void Init(const RenderContext& renderContext) = 0;
		virtual void Destroy(const RenderContext& renderContext) = 0;
		virtual void RecordCommandBuffer(const RenderFrameContext& renderFrameContext) = 0;

	protected:
	private:
		RenderPass m_renderPass;


		// Render State
		VkDescriptorSetLayout m_descriptorSetLayout;
		VkPipelineLayout m_pipelineLayout;
		VkPipeline m_pipeline;
	};
}