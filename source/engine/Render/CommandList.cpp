#include "Render/CommandList.h"
#include "Core/Logger.h"
#include "Render/InitVulkanTypes.h"
#include "Render/RenderTarget.h"
#include "Render/Shader.h"

//#define DUMP_BARRIERS
#ifdef DUMP_BARRIERS
#define logfbarrier(fmt, ...) logfinfo("[Barrier] " fmt, __VA_ARGS__)
#else
#define logfbarrier(fmt, ...) DUMMY_MACRO
#endif


namespace Mist
{
    VkQueueFlags GetVkQueueFlags(EQueueType type)
    {
        VkQueueFlags flags = 0;
        if (type & QUEUE_COMPUTE) flags |= VK_QUEUE_COMPUTE_BIT;
        if (type & QUEUE_GRAPHICS) flags |= VK_QUEUE_GRAPHICS_BIT;
        if (type & QUEUE_TRANSFER) flags |= VK_QUEUE_TRANSFER_BIT;
        if (type & VK_QUEUE_SPARSE_BINDING_BIT) flags |= VK_QUEUE_SPARSE_BINDING_BIT;
        return flags;
    }



    FamilyQueueIndexFinder::FamilyQueueIndexFinder(const RenderContext& context)
        : m_context(&context)
    {
        check(context.GPUDevice != VK_NULL_HANDLE);

        // Find the family queue that supports the requested type
        uint32_t numFamilyQueues = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(context.GPUDevice, &numFamilyQueues, nullptr);
        check(numFamilyQueues);
        m_properties.Allocate(numFamilyQueues);
        m_properties.Resize(numFamilyQueues);
        vkGetPhysicalDeviceQueueFamilyProperties(context.GPUDevice, &numFamilyQueues, m_properties.GetData());
    }

    FamilyQueueIndexFinder::~FamilyQueueIndexFinder()
    {
        m_properties.Delete();
        m_properties.Clear();
        m_context = nullptr;
    }

    uint32_t FamilyQueueIndexFinder::FindFamilyQueueIndex(EQueueType type)
    {
        VkQueueFlags flags = GetVkQueueFlags(type);
        for (uint32_t i = 0; i < m_properties.GetSize(); ++i)
        {
            if ((m_properties[i].queueFlags & flags) == flags)
                return i;
        }
        return InvalidFamilyQueueIndex;
    }

    void FamilyQueueIndexFinder::DumpInfo() const
    {
        for (uint32_t i = 0; i < m_properties.GetSize(); ++i)
        {
            logfok("Queue family (Index:%d;Count:%d) with flags (GRAPHICS:%d;COMPUTE:%d;TRANSFER:%d;SPARSE_BINDING:%d).\n",
                i, m_properties[i].queueCount,
                m_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ? 1 : 0,
                m_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT ? 1 : 0,
                m_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT ? 1 : 0,
                m_properties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ? 1 : 0);
        }
    }

    uint32_t FamilyQueueIndexFinder::FindFamilyQueueIndex(const RenderContext& context, EQueueType type)
    {
        FamilyQueueIndexFinder finder(context);
        return finder.FindFamilyQueueIndex(type);
    }

    VkImageSubresourceRange ConvertImageSubresourceRange(const cTexture* texture, const SubresourceRange& range)
    {
        VkImageSubresourceRange res;
        if (range.LayerCount == SubresourceRange::RemainingRange)
        {
            res.baseArrayLayer = 0;
            res.layerCount = VK_REMAINING_ARRAY_LAYERS;
        }
        else
        {
            res.baseArrayLayer = range.BaseArrayLayer;
            res.layerCount = range.LayerCount;
        }
        if (range.LevelCount == SubresourceRange::RemainingRange)
        {
            res.baseMipLevel = 0;
            res.levelCount = VK_REMAINING_MIP_LEVELS;
        }
        else
        {
            res.baseMipLevel = range.BaseMipLevel;
            res.levelCount = range.LevelCount;
        }
        res.aspectMask = 0;
        if (texture->HasDepth())
            res.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (texture->HasStencil())
            res.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (!res.aspectMask)
            res.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        return res;
    }

    struct ImageLayoutState
    {
        VkImageLayout Layout;
        VkAccessFlags2 AccessFlags;
        VkPipelineStageFlags2 StageFlags;
    };

    ImageLayoutState ConvertImageLayout(EImageLayout layout)
    {
        ImageLayoutState state = {};
        switch (layout)
        {
        case IMAGE_LAYOUT_UNDEFINED:
            state.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
            state.AccessFlags = 0;
            state.StageFlags = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            break;
        case IMAGE_LAYOUT_GENERAL:
            state.Layout = VK_IMAGE_LAYOUT_GENERAL;
            state.AccessFlags = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
            state.StageFlags = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            break;
        case IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            state.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            state.AccessFlags = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            state.StageFlags = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            state.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            state.AccessFlags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            state.StageFlags = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            break;
        case IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            state.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            state.AccessFlags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            state.StageFlags = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            break;
        case IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            state.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            state.AccessFlags = VK_ACCESS_2_SHADER_READ_BIT;
            state.StageFlags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            break;
        case IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            state.Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            state.AccessFlags = VK_ACCESS_2_TRANSFER_READ_BIT;
            state.StageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;
        case IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            state.Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            state.AccessFlags = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            state.StageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;
        case IMAGE_LAYOUT_PRESENT_SRC_KHR:
            state.Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            state.AccessFlags = 0;
            state.StageFlags = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            break;
        default:
            unreachable_code();
        }
        return state;
    }

    VkImageMemoryBarrier2 ConvertTextureBarrier(const TextureBarrier& barrier)
    {
        ImageLayoutState oldState = ConvertImageLayout(barrier.OldLayout);
        ImageLayoutState newState = ConvertImageLayout(barrier.NewLayout);
        VkImageMemoryBarrier2 imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, nullptr };
        imageBarrier.image = barrier.Texture->GetNativeImage();
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.srcAccessMask = oldState.AccessFlags;
        imageBarrier.dstAccessMask = newState.AccessFlags;
        imageBarrier.oldLayout = oldState.Layout;
        imageBarrier.newLayout = newState.Layout;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | oldState.StageFlags;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | newState.StageFlags;
        imageBarrier.subresourceRange = ConvertImageSubresourceRange(barrier.Texture, barrier.Subresource);
        return imageBarrier;
    }

    void CommandBuffer::Begin()
    {
        RenderAPI::ResetCommandBuffer(CmdBuffer, 0);
        RenderAPI::BeginCommandBuffer(CmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    void CommandBuffer::End()
    {
        RenderAPI::EndCommandBuffer(CmdBuffer);
    }

    CommandQueue::CommandQueue(const RenderContext* context, EQueueType type)
        : m_context(context),
        m_type(type),
        m_submissionId(0),
        m_lastFinishedId(0)
    {
        check(m_context && type != QUEUE_NONE);

        uint32_t queueFamilyIndex = FamilyQueueIndexFinder::FindFamilyQueueIndex(*context, m_type);
        m_queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(context->Device, queueFamilyIndex, 0, &m_queue);
        check(m_queue != VK_NULL_HANDLE);
        m_queueFamilyIndex = queueFamilyIndex;

        // Create semaphore type with a pNext chain in VkSemaphoreCreateInfo.
        VkSemaphoreTypeCreateInfo typeInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr };
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = 0;
        VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &typeInfo, 0};
        vkcheck(vkCreateSemaphore(m_context->Device, &semaphoreInfo, nullptr, &m_trackingSemaphore));

        m_createdBuffers.Allocate(16);
        m_submittedCmds.Allocate(16);
        m_commandPool.Allocate(16);
    }

    CommandQueue::~CommandQueue()
    {
        vkDestroySemaphore(m_context->Device, m_trackingSemaphore, nullptr);
        for (uint32_t i = 0; i < m_createdBuffers.GetSize(); ++i)
        {
            vkDestroyCommandPool(m_context->Device, m_createdBuffers[i]->Pool, nullptr);
            delete m_createdBuffers[i];
        }
        m_createdBuffers.Delete();
        m_submittedCmds.Delete();
        m_commandPool.Delete();
    }

    CommandBuffer* CommandQueue::CreateCommandBuffer()
    {
        CommandBuffer* cb = nullptr;
        if (!m_commandPool.IsEmpty())
        {
            cb = m_commandPool.Back();
            m_commandPool.Pop();
            check(cb->SubmissionId == UINT32_MAX);
            check(cb->Type == m_type);
        }
        else
        {
            cb = _new CommandBuffer();
            VkCommandPoolCreateInfo poolInfo = vkinit::CommandPoolCreateInfo(m_queueFamilyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
            vkcheck(vkCreateCommandPool(m_context->Device, &poolInfo, nullptr, &cb->Pool));
            VkCommandBufferAllocateInfo allocInfo = vkinit::CommandBufferCreateAllocateInfo(cb->Pool);
            vkcheck(vkAllocateCommandBuffers(m_context->Device, &allocInfo, &cb->CmdBuffer));
            cb->SubmissionId = UINT32_MAX;
            cb->Type = m_type;
            
            m_createdBuffers.Push(cb);
        }
        return cb;
    }

    uint64_t CommandQueue::Submit(CommandBuffer* const* cmds, uint32_t count)
    {
        check(cmds && count);
        check(m_signalSemaphores.GetSize() == m_signalSemaphoreValues.GetSize());
        check(m_waitSemaphores.GetSize() == m_waitSemaphoreValues.GetSize());

        // New submission
        ++m_submissionId;

        constexpr uint32_t MaxCount = 8;
        check(MaxCount >= count);

        // Generate array of command buffers to submit
        tStaticArray<VkCommandBuffer, MaxCount> commandBuffers;
        for (uint32_t i = 0; i < count; ++i)
        {
            check(cmds[i] && cmds[i]->CmdBuffer != VK_NULL_HANDLE);
            check(cmds[i]->SubmissionId == UINT32_MAX);
            cmds[i]->SubmissionId = m_submissionId;
            m_submittedCmds.Push(cmds[i]);
            commandBuffers.Push(cmds[i]->CmdBuffer);
        }
        tStaticArray<VkPipelineStageFlags, MaxCount> pipelineFlags;
        for (uint32_t i = 0; i < m_waitSemaphores.GetSize(); ++i)
        {
            pipelineFlags.Push(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);   
        }

        // Build sync structures
        VkTimelineSemaphoreSubmitInfo timelineInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr };

        // Add our tracking semaphore to watch the last submission.
        AddSignalSemaphore(m_trackingSemaphore, m_submissionId);

        timelineInfo.pSignalSemaphoreValues = m_signalSemaphoreValues.GetData();
        timelineInfo.pWaitSemaphoreValues = m_waitSemaphoreValues.GetData();
        timelineInfo.signalSemaphoreValueCount = m_signalSemaphoreValues.GetSize();
        timelineInfo.waitSemaphoreValueCount = m_waitSemaphoreValues.GetSize();
        

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.signalSemaphoreCount = m_signalSemaphores.GetSize();
        submitInfo.pSignalSemaphores = m_signalSemaphores.GetData();
        submitInfo.waitSemaphoreCount = m_waitSemaphores.GetSize();
        submitInfo.pWaitSemaphores = m_waitSemaphores.GetData();
        submitInfo.commandBufferCount = count;
        submitInfo.pCommandBuffers = commandBuffers.GetData();
        submitInfo.pWaitDstStageMask = pipelineFlags.GetData();
        vkcheck(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE));

        m_signalSemaphores.Clear();
        m_signalSemaphoreValues.Clear();
        m_waitSemaphores.Clear();
        m_waitSemaphoreValues.Clear();
        return m_submissionId;
    }

    void CommandQueue::AddWaitSemaphore(VkSemaphore semaphore, uint64_t value)
    {
        check(semaphore != VK_NULL_HANDLE);
        m_waitSemaphores.Push(semaphore);
        m_waitSemaphoreValues.Push(value);
    }

    void CommandQueue::AddSignalSemaphore(VkSemaphore semaphore, uint64_t value)
    {
        check(semaphore != VK_NULL_HANDLE);
        m_signalSemaphores.Push(semaphore);
        m_signalSemaphoreValues.Push(value);
    }

    void CommandQueue::AddWaitQueue(const CommandQueue* queue, uint64_t value)
    {
        check(queue);
        AddWaitSemaphore(queue->m_trackingSemaphore, value);
    }

    uint64_t CommandQueue::QueryTrackingId()
    {
        vkGetSemaphoreCounterValue(m_context->Device, m_trackingSemaphore, &m_lastFinishedId);
        return m_lastFinishedId;
    }

    void CommandQueue::ProcessInFlightCommands()
    {
        if (m_submittedCmds.IsEmpty())
            return;
        uint64_t lastSignal = QueryTrackingId();
        tFixedHeapArray<CommandBuffer*> cmds = m_submittedCmds.Move();
        m_submittedCmds.Allocate(cmds.GetReservedSize());
        
        for (uint32_t i = 0; i < cmds.GetSize(); ++i)
        {
            CommandBuffer* cmd = cmds[i];
            if (cmd->SubmissionId <= lastSignal)
            {
                m_commandPool.Push(cmd);
                cmd->SubmissionId = UINT32_MAX;
            }
            else
            {
                m_submittedCmds.Push(cmd);
            }
        }
    }

    bool CommandQueue::PollCommandSubmission(uint64_t submissionId)
    {
        if (m_submissionId < submissionId || !submissionId)
            return false;
        return QueryTrackingId() >= submissionId;
    }

    bool CommandQueue::WaitForCommandSubmission(uint64_t submissionId, uint64_t timeout)
    {
        if (m_submissionId < submissionId || !submissionId)
            return false;
        if (PollCommandSubmission(submissionId))
            return true;

        VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, nullptr};
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_trackingSemaphore;
        waitInfo.pValues = &submissionId;
        VkResult res = vkWaitSemaphores(m_context->Device, &waitInfo, timeout * 1000000000);
        return false;
    }
    CommandList::CommandList(const RenderContext* context)
        : m_context(context),
        m_currentCmd(nullptr), 
        m_graphicsState({})
    {
        check(m_context);
    }

    CommandList::~CommandList()
    {
        check(!IsRecording());
    }

    uint64_t CommandList::ExecuteCommandLists(CommandList* const* lists, uint32_t count)
    {
        static constexpr uint32_t MaxLists = 8;
        check(lists && count <= MaxLists);
        tStaticArray<CommandBuffer*, MaxLists> cmds;
        const RenderContext* context = lists[0]->m_context;
        for (uint32_t i = 0; i < count; ++i)
        {
            check(lists[i] && lists[i]->GetCurrentCommandBuffer());
            cmds.Push(lists[i]->GetCurrentCommandBuffer());
            // Check all contexts are the same
            check(context == lists[i]->m_context);
            lists[i]->Executed();
        }

        CommandQueue* queue = context->Queue;
        uint64_t submission = queue->Submit(cmds.GetData(), count);
        return submission;
    }

    void CommandList::Begin()
    {
        check(m_context);
        check(!IsRecording());
        // TODO: add support to all types of queues, cache queue type per command list.
        CommandQueue* queue = m_context->Queue;
        m_currentCmd = queue->CreateCommandBuffer();
        m_currentCmd->Begin();

        ClearState();
    }

    void CommandList::End()
    {
        check(IsRecording());
        if (CheckGraphicsCommandType())
            EndRenderPass();
        m_currentCmd->End();
    }

    void CommandList::BeginMarker(const char* name)
    {
        check(m_currentCmd);
        BeginGPUEvent(*m_context, m_currentCmd->CmdBuffer, name);
    }

    void CommandList::EndMarker()
    {
        check(m_currentCmd);
        EndGPUEvent(*m_context, m_currentCmd->CmdBuffer);
    }

    void CommandList::SetGraphicsState(const GraphicsState& state)
    {
        check(CheckGraphicsCommandType());
        if (state == m_graphicsState)
            return;
        if (m_state != STATE_GRAPHICS)
            ClearState();

        // End render pass if needed
        if (state.Rt != m_graphicsState.Rt)
            EndRenderPass();

        // Begin render pass if needed
        if (!m_graphicsState.Rt)
        {
            state.Rt->BeginPass(*m_context, m_currentCmd->CmdBuffer);
        }

        // Bind program
        if (state.Program && state.Program != m_graphicsState.Program)
        {
            //state.Program->UseProgram(*m_context, m_currentCmd->CmdBuffer);
        }

        // Bind vertex buffer
        if (state.Vbo.IsAllocated())
            BindVertexBuffer(state.Vbo);
        // Bind index buffer
        if (state.Ibo.IsAllocated())
            BindIndexBuffer(state.Ibo);

        m_graphicsState = state;
        m_state = STATE_GRAPHICS;
    }

    void CommandList::ClearColor(float r, float g, float b, float a)
    {
        check(CheckGraphicsCommandType());
        if (m_graphicsState.Rt)
        {
            m_graphicsState.Rt->ClearColor(m_currentCmd->CmdBuffer, r, g, b, a);
        }
    }

    void CommandList::ClearDepthStencil(float depth, uint32_t stencil)
    {
        check(CheckGraphicsCommandType());
        if (m_graphicsState.Rt)
        {
            m_graphicsState.Rt->ClearDepthStencil(m_currentCmd->CmdBuffer, depth, stencil);
        }
    }

    void CommandList::BindVertexBuffer(VertexBuffer vbo)
    {
        check(CheckGraphicsCommandType() && vbo.IsAllocated());
        if (vbo == m_graphicsState.Vbo)
            return;
        vbo.Bind(m_currentCmd->CmdBuffer);
        m_graphicsState.Vbo = vbo;
    }

    void CommandList::BindIndexBuffer(IndexBuffer ibo)
    {
        check(CheckGraphicsCommandType() && ibo.IsAllocated());
        if (ibo == m_graphicsState.Ibo)
            return;
        ibo.Bind(m_currentCmd->CmdBuffer);
        m_graphicsState.Ibo = ibo;
    }

    void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        check(CheckGraphicsCommandType());
        //m_graphicsState.Program->FlushDescriptors(*m_context);
        RenderAPI::CmdDraw(m_currentCmd->CmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset)
    {
        check(CheckGraphicsCommandType());
        //m_graphicsState.Program->FlushDescriptors(*m_context);
        RenderAPI::CmdDrawIndexed(m_currentCmd->CmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, 0);
    }

    void CommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
    {
        check(CheckGraphicsCommandType());
        RenderAPI::CmdSetViewport(GetCurrentCommandBuffer()->CmdBuffer, width, height, minDepth, maxDepth, x, y);
    }

    void CommandList::SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        check(CheckGraphicsCommandType());
        RenderAPI::CmdSetScissor(GetCurrentCommandBuffer()->CmdBuffer, width, height, x, y);
    }

    void CommandList::SetComputeState(const ComputeState& state)
    {
        check(CheckComputeCommandType());
        if (state == m_computeState)
            return;
        EndRenderPass();
        if (m_state != STATE_COMPUTE)
            ClearState();
        if (state.Program != m_computeState.Program)
        {
            //state.Program->UseProgram(*m_context, m_currentCmd->CmdBuffer);
        }
        m_computeState = state;
        m_state = STATE_COMPUTE;
    }

    void CommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        check(CheckComputeCommandType());
        //m_computeState.Program->FlushDescriptors(*m_context);
        RenderAPI::CmdDispatch(m_currentCmd->CmdBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void CommandList::CopyBuffer(AllocatedBuffer src, AllocatedBuffer dst, uint32_t size, uint32_t srcOffset, uint32_t dstOffset)
    {
        check(CheckTransferCommandType());
        check(src.IsAllocated() && dst.IsAllocated());
        VkBufferCopy copyRegion = {};
        copyRegion.dstOffset = dstOffset;
        copyRegion.srcOffset = srcOffset;
        copyRegion.size = size;
        vkCmdCopyBuffer(m_currentCmd->CmdBuffer, src.Buffer, dst.Buffer, 1, &copyRegion);
    }

    void CommandList::SetTextureState(const TextureBarrier* barriers, uint32_t count)
    {
        check(count <= MaxBarriers);
        tStaticArray<VkImageMemoryBarrier2, MaxBarriers> imageBarriers;
        for (uint32_t i = 0; i < count; ++i)
        {
            imageBarriers.Push(ConvertTextureBarrier(barriers[i]));
        }
        VkDependencyInfo depInfo = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
        depInfo.imageMemoryBarrierCount = count;
        depInfo.pImageMemoryBarriers = imageBarriers.GetData();
        vkCmdPipelineBarrier2(m_currentCmd->CmdBuffer, &depInfo);
    }

    void CommandList::SetTextureState(const TextureBarrier& barrier)
    {
        // TODO: cache all barriers and flush them before draw or dispatch.
        VkImageMemoryBarrier2 imageBarrier = ConvertTextureBarrier(barrier);
        logfbarrier("Setting texture state from %s to %s [%s]\n", 
            ImageLayoutToStr(barrier.OldLayout), ImageLayoutToStr(barrier.NewLayout),
            barrier.Texture->GetName());

        VkDependencyInfo depInfo = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;
        vkCmdPipelineBarrier2(m_currentCmd->CmdBuffer, &depInfo);
    }

    void CommandList::BindDescriptorSets(const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsetArray, uint32_t dynamicOffsetCount)
    {
        ShaderProgram* program = GetCurrentProgram();
        if (program)
        {
            check(IsRecording());
            //program->BindDescriptorSets(m_currentCmd->CmdBuffer, setArray, setCount, firstSet, dynamicOffsetArray, dynamicOffsetCount);
        }
    }

    void CommandList::BindProgramDescriptorSets()
    {
        ShaderProgram* program = GetCurrentProgram();
        if (program)
        {
            check(IsRecording());
            //program->FlushDescriptors(*m_context);
        }
    }

    void CommandList::ClearState()
    {
        EndRenderPass();
        m_graphicsState = {};
        m_computeState = {};
    }

    void CommandList::EndRenderPass()
    {
        check(CheckGraphicsCommandType());
        if (m_graphicsState.Rt)
        {
            m_graphicsState.Rt->EndPass(m_currentCmd->CmdBuffer);
            m_graphicsState.Rt = nullptr;
        }
    }
    void CommandList::Executed()
    {
        m_currentCmd = nullptr;
    }
}