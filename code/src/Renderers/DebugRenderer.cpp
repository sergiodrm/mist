// Autogenerated code for vkmmc project
// Source file
#include "DebugRenderer.h"
#include "Debug.h"
#include "InitVulkanTypes.h"
#include "Shader.h"
#include "Globals.h"
#include "VulkanRenderEngine.h"

namespace vkmmc
{
    void DebugRenderer::Init(const RendererCreateInfo& info)
    {
		/**********************************/
		/** Pipeline layout and pipeline **/
		/**********************************/
        ShaderDescription descriptions[] =
        {
            {.Filepath = globals::LineVertexShader, .Stage = VK_SHADER_STAGE_VERTEX_BIT},
            {.Filepath = globals::LineFragmentShader, .Stage = VK_SHADER_STAGE_FRAGMENT_BIT}
        };
        uint32_t descriptionCount = sizeof(descriptions) / sizeof(ShaderDescription);

        VertexInputLayout inputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float3 });
        m_renderPipeline = RenderPipeline::Create(info.RContext, info.RenderPass, 0,
            descriptions, descriptionCount, inputLayout, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

		// Uniform descriptor
        glm::vec4 color = { 1.f, 0.f, 0.f, 1.f };
		m_uniformBuffer = Memory::CreateBuffer(info.RContext.Allocator, sizeof(glm::vec4),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        Memory::MemCopyDataToBuffer(info.RContext.Allocator, m_uniformBuffer.Alloc, &color, sizeof(glm::vec4));
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = m_uniformBuffer.Buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(glm::vec4);
		DescriptorBuilder::Create(*info.LayoutCache, *info.DescriptorAllocator)
			.BindBuffer(0, bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(info.RContext, m_uniformSet);

        // VertexBuffer
        glm::vec3 pos[2] = { glm::vec3{0.f}, glm::vec3{10.f} };
        BufferCreateInfo vbInfo;
        vbInfo.RContext = info.RContext;
        vbInfo.Size = sizeof(glm::vec3) * 2;
        vbInfo.Data = pos;
        m_lineVertexBuffer.Init(vbInfo);
    }

    void DebugRenderer::Destroy(const RenderContext& renderContext)
    {
        m_lineVertexBuffer.Destroy(renderContext);
        Memory::DestroyBuffer(renderContext.Allocator, m_uniformBuffer);
        m_renderPipeline.Destroy(renderContext);
    }

    void DebugRenderer::RecordCommandBuffer(const RenderFrameContext& renderFrameContext)
    {
        vkCmdBindPipeline(renderFrameContext.GraphicsCommand, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderPipeline.GetPipelineHandle());
        VkDescriptorSet sets[] = { renderFrameContext.GlobalDescriptorSet, m_uniformSet };
        uint32_t setCount = sizeof(sets) / sizeof(VkDescriptorSet);
        vkCmdBindDescriptorSets(renderFrameContext.GraphicsCommand, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_renderPipeline.GetPipelineLayoutHandle(), 0, 2, sets, 0, nullptr);
        m_lineVertexBuffer.Bind(renderFrameContext.GraphicsCommand);
        vkCmdDraw(renderFrameContext.GraphicsCommand, 2, 1, 0, 0);
    }

}