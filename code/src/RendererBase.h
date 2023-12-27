// header file for vkmmc project 

#include "RenderTypes.h"
#include "RenderDescriptor.h"

namespace vkmmc
{
	struct RenderContext;
	class IRendererBase
	{
	public:
		IRendererBase(const RenderContext& renderContext);
		virtual ~IRendererBase() = default;

		virtual void RecordCommandBuffer(VkCommandBuffer cmd, size_t imageIndex) = 0;

	protected:
		void BeginRenderPass(VkCommandBuffer cmd, size_t imageIndex);
		bool CreateUniformBuffer(size_t bufferSize);
	private:
		// Info about current render context
		RenderContext m_renderContext;
		uint32_t m_framebufferWidth;
		uint32_t m_framebufferHeight;

		// Descriptors info
		VkDescriptorSetLayout m_descriptorSetLayout;
		DescriptorAllocator m_descriptorAllocator;
		std::vector<VkDescriptorSet> m_descriptorSetArray;

		// Render State
		// TODO: depth image
		VkRenderPass m_renderPass;
		VkPipelineLayout m_pipelineLayout;
		VkPipeline m_pipeline;

		// Uniform buffers
		std::vector<AllocatedBuffer> m_uniformBufferArray;
	};
}