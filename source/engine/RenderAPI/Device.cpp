#include "Device.h"
#include "Utils.h"
#include "Core/Logger.h"
#include "Application/CmdParser.h"

#include "vkbootstrap/VkBootstrap.h"
#include "Application/Application.h"
#include "Render/RenderTypes.h"
#include "Utils/GenericUtils.h"

#include "ShaderCompiler.h"

namespace Mist
{
    extern Mist::CBoolVar CVar_EnableValidationLayer;
    extern Mist::CBoolVar CVar_ExitValidationLayer;
}

namespace render
{
    uint32_t GVulkanLayerValidationErrors = 0;


    VkBool32 DebugVulkanCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        Mist::LogLevel level = Mist::LogLevel::Info;
        switch (severity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: level = Mist::LogLevel::Error; ++GVulkanLayerValidationErrors; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: level = Mist::LogLevel::Debug; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: level = Mist::LogLevel::Warn; break;
        }
        Logf(level, "\nValidation layer\n> Message: %s\n\n", callbackData->pMessage);
        if (level == Mist::LogLevel::Error)
        {
            if (!Mist::CVar_ExitValidationLayer.Get())
                Mist::Debug::PrintCallstack();
            check(!Mist::CVar_ExitValidationLayer.Get() && "Validation layer error");
        }
        return VK_FALSE;
    }

    bool VulkanContext::FormatSupportsLinearFiltering(Format format) const
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, utils::ConvertFormat(format), &props);
        return props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    }

    Semaphore::Semaphore(Device* device)
        : m_device(device), m_semaphore(VK_NULL_HANDLE), m_isTimeline(false)
    {
        check(m_device);
    }

    Semaphore::~Semaphore()
    {
        check(m_device);
        m_device->DestroyRenderSemaphore(this);
    }

    RenderTargetDescription& RenderTargetDescription::AddColorAttachment(TextureHandle texture, TextureSubresourceRange range)
    {
        Format format = texture->m_description.format;
        RenderTargetAttachment& attachment = colorAttachments.Push();
        check((format != Format_Undefined && !utils::IsDepthFormat(format) && !utils::IsStencilFormat(format)) 
            || (!utils::IsDepthFormat(texture->m_description.format) && !utils::IsStencilFormat(texture->m_description.format)));
        attachment.texture = texture;
        attachment.range = range;
        return *this;
    }

    RenderTargetDescription& RenderTargetDescription::SetDepthStencilAttachment(TextureHandle texture, TextureSubresourceRange range)
    {
        Format format = texture->m_description.format;
        check(utils::IsDepthFormat(format) || utils::IsStencilFormat(format));
        depthStencilAttachment.texture = texture;
        depthStencilAttachment.range = range;
        return *this;
    }

    Buffer::~Buffer()
    {
        check(m_device);
        m_device->DestroyBuffer(this);
    }

    Texture::~Texture()
    {
        check(m_device);
        m_device->DestroyTexture(this);
    }

    size_t Texture::GetImageSize() const
    {
        size_t factor = 1;
        size_t size = 0;
        switch (m_description.dimension)
        {
        case ImageDimension_Cube:
        case ImageDimension_CubeArray:
            factor = 1;
            [[fallthrough]];
        case ImageDimension_1D:
        case ImageDimension_2D:
        case ImageDimension_3D:
        case ImageDimension_1DArray:
        case ImageDimension_2DArray:
            size = m_description.extent.width * m_description.extent.height * m_description.extent.depth;
            size *= utils::GetBytesPerPixel(m_description.format);

            size_t w = __max(m_description.extent.width >> 1, 1);
            size_t h = __max(m_description.extent.height >> 1, 1);
            size_t d = __max(m_description.extent.depth >> 1, 1);

            for (uint32_t i = 1; i < m_description.mipLevels; ++i)
            {
                size += w * h * d * utils::GetBytesPerPixel(m_description.format);
                w = __max(w >> 1, 1);
                h = __max(h >> 1, 1);
                d = __max(d >> 1, 1);
            }

            size *= m_description.layers;
            break;
        }
        return size;
    }

    TextureView* Texture::GetView(const TextureViewDescription& viewDescription)
    {
        TextureViewDescription description = viewDescription;
        if (description.format == Format_Undefined)
            description.format = m_description.format;
        if (description.dimension == ImageDimension_Undefined)
            description.dimension = m_description.dimension;

        if (m_views.contains(description))
            return &m_views[description];

        TextureView view;
        view.m_description = description;
        view.m_image = m_image;
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.flags = 0;
        viewInfo.pNext = nullptr;
        viewInfo.image = m_image;
        viewInfo.viewType = utils::ConvertImageViewType(description.dimension);
        viewInfo.format = utils::ConvertFormat(description.format);
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange = utils::ConvertImageSubresourceRange(description.range, description.format);
        if (description.viewOnlyDepth && utils::IsDepthFormat(description.format))
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else if (description.viewOnlyStencil && utils::IsStencilFormat(description.format))
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        vkcheck(vkCreateImageView(m_device->GetContext().device, &viewInfo, m_device->GetContext().allocationCallbacks, &view.m_view));

        m_views.insert({ description, view });
        return &m_views[description];
    }


    Sampler::~Sampler()
    {
        check(m_device);
        m_device->DestroySampler(this);
    }

    Shader::~Shader()
    {
        check(m_device);
        m_device->DestroyShader(this);
    }

    VertexInputLayout VertexInputLayout::BuildVertexInputLayout(const VertexInputAttribute* attributes, uint32_t count)
    {
        check(count <= MaxAttributes);
        VertexInputLayout layout;
        layout.m_attributes.Resize(count);

        uint32_t offset = 0;
        for (uint32_t i = 0; i < count; i++)
        {
            VkVertexInputAttributeDescription& attribute = layout.m_attributes[i];
            attribute.format = utils::ConvertFormat(attributes[i].format);
            attribute.binding = 0;
            attribute.location = i;
            attribute.offset = offset;

            offset += utils::GetFormatSize(attributes[i].format);
            layout.m_description.Push(attributes[i]);
        }
        layout.m_binding.binding = 0;
        layout.m_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        layout.m_binding.stride = offset;
        return layout;
    }

    RenderTarget::~RenderTarget()
    {
        check(m_device);
        m_device->DestroyRenderTarget(this);
    }

    RenderTargetInfo::RenderTargetInfo(const RenderTargetDescription& description)
    {
        extent = { UINT32_MAX, UINT32_MAX };
        for (uint32_t i = 0; i < description.colorAttachments.GetSize(); ++i)
        {
            const RenderTargetAttachment& rta = description.colorAttachments[i];
            check(rta.IsValid() && rta.texture->m_description.format != Format_Undefined);
            colorFormats.Push(rta.texture->m_description.format);

            if (extent.width == UINT32_MAX && extent.height == UINT32_MAX)
            {
                extent.width = rta.texture->m_description.extent.width;
                extent.height = rta.texture->m_description.extent.height;
            }
            else
            {
                check(extent.width == rta.texture->m_description.extent.width);
                check(extent.height == rta.texture->m_description.extent.height);
            }
        }
        if (description.depthStencilAttachment.IsValid())
        {
            const RenderTargetAttachment& rta = description.depthStencilAttachment;
            check(rta.IsValid() && utils::IsDepthFormat(rta.texture->m_description.format));
            depthStencilFormat = rta.texture->m_description.format;

            if (extent.width == UINT32_MAX && extent.height == UINT32_MAX)
            {
                extent.width = rta.texture->m_description.extent.width;
                extent.height = rta.texture->m_description.extent.height;
            }
            else
            {
                check(extent.width == rta.texture->m_description.extent.width);
                check(extent.height == rta.texture->m_description.extent.height);
            }
        }
        check(extent.width != UINT32_MAX && extent.height != UINT32_MAX);
    }

    void RenderTarget::BeginPass(CommandBuffer* cmd, Rect2D renderArea)
    {
        check(m_renderPass != VK_NULL_HANDLE && m_framebuffer != VK_NULL_HANDLE);
        VkRenderPassBeginInfo info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .pNext = nullptr };
        info.renderPass = m_renderPass;
        info.framebuffer = m_framebuffer;
        info.renderArea = { .offset = {.x = renderArea.offset.x, .y = renderArea.offset.y }, .extent = { .width = renderArea.extent.width, .height = renderArea.extent.height } };
        info.pClearValues = nullptr;
        info.clearValueCount = 0;
        vkCmdBeginRenderPass(cmd->cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    void RenderTarget::BeginPass(CommandBuffer* cmd)
    {
        Rect2D area;
        area.extent = m_info.extent;
        area.offset = { 0,0 };
        BeginPass(cmd, area);
    }

    void RenderTarget::EndPass(CommandBuffer* cmd)
    {
        vkCmdEndRenderPass(cmd->cmd);
    }

    void RenderTarget::ClearColor(CommandBuffer* cmd, float r, float g, float b, float a)
    {
        if (!m_description.colorAttachments.IsEmpty())
        {
            Mist::tStaticArray<VkClearAttachment, RenderTargetDescription::MaxRenderAttachments> clears;
            Mist::tStaticArray<VkClearRect, RenderTargetDescription::MaxRenderAttachments> rects;
            for (uint32_t i = 0; i < m_description.colorAttachments.GetSize(); ++i)
            {
                VkClearAttachment& clear = clears.Push();
                clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                clear.clearValue = { .color = {r,g,b,a} };
                clear.colorAttachment = i;

                VkClearRect& rect = rects.Push();
                rect.rect = { .offset{0,0}, .extent{m_info.extent.width, m_info.extent.height} };
                rect.layerCount = 1;
                rect.baseArrayLayer = 0;
            }
            vkCmdClearAttachments(cmd->cmd, clears.GetSize(), clears.GetData(), rects.GetSize(), rects.GetData());
        }
    }

    void RenderTarget::ClearDepthStencil(CommandBuffer* cmd, float depth, uint32_t stencil)
    {
        if (m_description.depthStencilAttachment.IsValid())
        {
            VkClearAttachment clear;
            clear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (utils::IsDepthStencilFormat(m_description.depthStencilAttachment.texture->m_description.format))
                clear.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            clear.clearValue.depthStencil.depth = depth;
            clear.clearValue.depthStencil.stencil = stencil;
            clear.colorAttachment = 0;

            VkClearRect rect;
            rect.rect = { .offset{0,0}, .extent{m_info.extent.width, m_info.extent.height} };
            rect.layerCount = 1;
            rect.baseArrayLayer = 0;
            vkCmdClearAttachments(cmd->cmd, 1, &clear, 1, &rect);
        }
    }

    BindingLayout::~BindingLayout()
    {
        check(m_device);
        m_device->DestroyBindingLayout(this);
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        check(m_device);
        m_device->DestroyGraphicsPipeline(this);
    }

    void GraphicsPipeline::UsePipeline(CommandBuffer* cmd)
    {
        check(cmd && cmd->cmd && m_pipeline && m_pipelineLayout);
        vkCmdBindPipeline(cmd->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    }

    ComputePipeline::~ComputePipeline()
    {
        check(m_device);
        m_device->DestroyComputePipeline(this);
    }

    void CommandBuffer::Begin()
    {
        vkcheck(vkResetCommandBuffer(cmd, 0));
        VkCommandBufferBeginInfo info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = nullptr };
        info.pInheritanceInfo = nullptr;
        info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkcheck(vkBeginCommandBuffer(cmd, &info));
    }

    void CommandBuffer::End()
    {
        vkcheck(vkEndCommandBuffer(cmd));
    }

    CommandQueue::CommandQueue(Device* device, QueueType type)
        : m_device(device), m_type(type), m_queue(VK_NULL_HANDLE), m_submissionId(0), m_lastSubmissionIdFinished(0)
    {
        check(m_device && m_type != Queue_None);

        m_queueFamilyIndex = FindFamilyQueueIndex(m_device, m_type);
        check(m_queueFamilyIndex != UINT32_MAX);
        vkGetDeviceQueue(m_device->GetContext().device, m_queueFamilyIndex, 0, &m_queue);
        check(m_queue != VK_NULL_HANDLE);

        m_trackingSemaphore = m_device->CreateRenderSemaphore(true);
        m_device->SetDebugName(m_trackingSemaphore.GetPtr(), "tracking semaphore command queue");
    }

    CommandQueue::~CommandQueue()
    {
        for (uint32_t i = 0; i < (uint32_t)m_createdCommandBuffers.size(); ++i)
        {
            vkDestroyCommandPool(m_device->GetContext().device, m_createdCommandBuffers[i]->pool, m_device->GetContext().allocationCallbacks);
            delete m_createdCommandBuffers[i];
        }
        m_createdCommandBuffers.clear();
        m_signalSemaphores.Clear();
        m_waitSemaphores.Clear();
        m_trackingSemaphore = nullptr;
    }

    CommandBuffer* CommandQueue::CreateCommandBuffer()
    {
        CommandBuffer* cmd = nullptr;
        if (!m_commandPool.empty())
        {
            cmd = m_commandPool.back();
            m_commandPool.pop_back();
            check(cmd->submissionId == UINT32_MAX);
            check(cmd->type == m_type);
        }
        else
        {
            cmd = _new CommandBuffer();
            VkCommandPoolCreateInfo poolInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = nullptr };
            poolInfo.queueFamilyIndex = m_queueFamilyIndex;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            vkcheck(vkCreateCommandPool(m_device->GetContext().device, &poolInfo, m_device->GetContext().allocationCallbacks, &cmd->pool));
            VkCommandBufferAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = nullptr };
            allocInfo.commandPool = cmd->pool;
            allocInfo.commandBufferCount = 1;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            vkcheck(vkAllocateCommandBuffers(m_device->GetContext().device, &allocInfo, &cmd->cmd));

            cmd->submissionId = UINT32_MAX;
            cmd->type = m_type;

            m_createdCommandBuffers.push_back(cmd);
        }

        return cmd;
    }

    uint64_t CommandQueue::SubmitCommandBuffers(CommandBuffer* const* cmds, uint32_t count)
    {
        check(cmds && count);
        check(m_waitSemaphoreValues.GetSize() == m_waitSemaphores.GetSize());
        check(m_signalSemaphoreValues.GetSize() == m_signalSemaphores.GetSize());

        // Create a new submission id.
        ++m_submissionId;

        // Signal internal semaphore to know when this submission is finished.
        AddSignalSemaphore(m_trackingSemaphore, m_submissionId);

        constexpr uint32_t MaxCount = 8;
        check(count <= MaxCount);

        // Generate array of command buffers
        Mist::tStaticArray<VkCommandBuffer, MaxCount> commandBuffers;
        for (uint32_t i = 0; i < count; ++i)
        {
            check(cmds[i] && cmds[i] != VK_NULL_HANDLE);
            check(cmds[i]->submissionId == UINT32_MAX);
            check(cmds[i]->type == m_type);
            cmds[i]->submissionId = m_submissionId;
            m_submittedCommandBuffers.push_back(cmds[i]);
            commandBuffers.Push(cmds[i]->cmd);
        }
        Mist::tStaticArray<VkPipelineStageFlags, MaxSemaphores> pipelineFlags;
        for (uint32_t i = 0; i < m_waitSemaphores.GetSize(); ++i)
            pipelineFlags.Push(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

        // Collect semaphores native objects
        Mist::tStaticArray<VkSemaphore, MaxCount> waitSemaphores;
        Mist::tStaticArray<VkSemaphore, MaxCount> signalSemaphores;
        for (uint32_t i = 0; i < m_waitSemaphores.GetSize(); ++i)
            waitSemaphores.Push(m_waitSemaphores[i]->m_semaphore);
        for (uint32_t i = 0; i < m_signalSemaphores.GetSize(); ++i)
            signalSemaphores.Push(m_signalSemaphores[i]->m_semaphore);

        // Create syncronization values struct
        VkTimelineSemaphoreSubmitInfo timelineInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr };
        timelineInfo.signalSemaphoreValueCount = m_signalSemaphoreValues.GetSize();
        timelineInfo.pSignalSemaphoreValues = m_signalSemaphoreValues.GetData();
        timelineInfo.waitSemaphoreValueCount = m_waitSemaphoreValues.GetSize();
        timelineInfo.pWaitSemaphoreValues = m_waitSemaphoreValues.GetData();

        // Submit info
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
        submitInfo.pNext = &timelineInfo;
        submitInfo.signalSemaphoreCount = signalSemaphores.GetSize();
        submitInfo.pSignalSemaphores = signalSemaphores.GetData();
        submitInfo.waitSemaphoreCount = waitSemaphores.GetSize();
        submitInfo.pWaitSemaphores = waitSemaphores.GetData();
        submitInfo.commandBufferCount = commandBuffers.GetSize();
        submitInfo.pCommandBuffers = commandBuffers.GetData();
        submitInfo.pWaitDstStageMask = pipelineFlags.GetData();
        vkcheck(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE));

        // Clear sync info
        m_waitSemaphores.Clear();
        m_waitSemaphoreValues.Clear();
        m_signalSemaphores.Clear();
        m_signalSemaphoreValues.Clear();

        return m_submissionId;
    }

    void CommandQueue::AddWaitSemaphore(SemaphoreHandle semaphore, uint64_t value)
    {
        check(semaphore != VK_NULL_HANDLE);
        m_waitSemaphores.Push(semaphore);
        m_waitSemaphoreValues.Push(value);
    }

    void CommandQueue::AddSignalSemaphore(SemaphoreHandle semaphore, uint64_t value)
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
        m_lastSubmissionIdFinished = m_device->GetSemaphoreTimelineCounter(m_trackingSemaphore);
        return m_lastSubmissionIdFinished;
    }

    void CommandQueue::ProcessInFlightCommands()
    {
        if (m_submittedCommandBuffers.empty())
            return;

        uint64_t lastFinishedId = QueryTrackingId();

        // iterate submitted cmd array backward, so we can reorder the unused slots in the same iteration
        for (uint32_t i = (uint32_t)m_submittedCommandBuffers.size() - 1; i < (uint32_t)m_submittedCommandBuffers.size(); --i)
        {
            CommandBuffer* cmd = m_submittedCommandBuffers[i];
            check(cmd);
            // If we got a signal after the submission of the command, 
            // add it to command pool and fill the hole with the last command buffer in the array
            if (cmd->submissionId <= lastFinishedId)
            {
                m_commandPool.push_back(cmd);
                cmd->submissionId = UINT32_MAX;

                if (i != (uint32_t)m_submittedCommandBuffers.size() - 1)
                {
                    // Overwrite with the last element of the array
                    CommandBuffer* chosenOne = m_submittedCommandBuffers.back();
                    m_submittedCommandBuffers[i] = chosenOne;
                }
                m_submittedCommandBuffers.pop_back();
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

        VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, nullptr };
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_trackingSemaphore->m_semaphore;
        waitInfo.pValues = &submissionId;
        VkResult res = vkWaitSemaphores(m_device->GetContext().device, &waitInfo, timeout);
        return res == VK_SUCCESS;
    }

    TransferMemory::TransferMemory(Device* device, BufferHandle buffer, uint64_t pointer, uint64_t availableSize)
        : m_device(device), m_buffer(buffer), m_pointer(pointer), m_dst(nullptr), m_availableSize(availableSize)
    {
        check(m_device && m_buffer);
        check(m_buffer->m_description.bufferUsage & BufferUsage_TransferSrc && m_buffer->m_description.memoryUsage == MemoryUsage_CpuToGpu);
    }

    TransferMemory::~TransferMemory()
    {
        UnmapMemory();
    }

    void TransferMemory::MapMemory()
    {
        if (!m_dst)
        {
            vkcheck(vmaMapMemory(m_device->GetContext().memoryContext.allocator, m_buffer->m_alloc, &m_dst));
            check(m_dst);
        }
    }

    void TransferMemory::UnmapMemory()
    {
        if (m_dst)
        {
            vmaUnmapMemory(m_device->GetContext().memoryContext.allocator, m_buffer->m_alloc);
            m_dst = nullptr;
        }
    }

    void TransferMemory::Write(const void* data, size_t size, size_t srcOffset, size_t dstOffset)
    {
        check(m_dst);
        check(dstOffset + size + m_pointer <= m_buffer->m_description.size);
        check(dstOffset + size <= m_availableSize);
        size_t offset = dstOffset + m_pointer;
        void* dst = reinterpret_cast<char*>(m_dst) + offset;
        const void* src = reinterpret_cast<const char*>(data) + srcOffset;
        memcpy_s(dst, m_availableSize-dstOffset, src, size);
    }

    TransferMemoryPool::TransferMemoryPool(Device* device, uint64_t defaultChunkSize)
        : m_device(device), m_defaultChunkSize(utils::Align(defaultChunkSize, Alignment)), m_currentChunkIndex(InvalidChunkIndex)
    { 
        m_pool.reserve(8);
        m_submittedChunkIndices.reserve(8);
    }

    TransferMemory TransferMemoryPool::Suballocate(uint64_t size)
    {
        uint64_t alignedSize = utils::Align(size, Alignment);
        uint32_t chunkToSubmit = InvalidChunkIndex;

        // Chunk opened, check if it has space enough for the request
        if (m_currentChunkIndex != InvalidChunkIndex)
        {
            check(m_currentChunkIndex < (uint32_t)m_pool.size());
            Chunk& currentChunk = m_pool[m_currentChunkIndex];
            uint64_t finalPointer = alignedSize + currentChunk.pointer;

            if (finalPointer <= currentChunk.buffer->m_description.size)
            {
                uint64_t pointer = currentChunk.pointer;
                currentChunk.pointer = finalPointer;
                return TransferMemory(m_device, currentChunk.buffer, pointer, alignedSize);
            }

            // not enough space
            chunkToSubmit = m_currentChunkIndex;
            m_currentChunkIndex = InvalidChunkIndex;
        }

        check(m_currentChunkIndex == InvalidChunkIndex);

        // Find a submitted chunk with old version
        uint64_t lastCompletedSubmissionId = m_device->GetCommandQueue(Queue_Transfer)->GetLastSubmissionIdFinished();
        for (uint32_t i = 0; i < (uint32_t)m_pool.size(); ++i)
        {
            if (m_pool[i].version <= lastCompletedSubmissionId && m_pool[i].buffer->m_description.size >= alignedSize)
            {
                m_currentChunkIndex = i;
                Chunk& currentChunk = m_pool[i];
                currentChunk.version = UINT64_MAX;
                currentChunk.pointer = 0;
                break;
            }
        }

        // Add completed chunk to the pool
        if (chunkToSubmit != InvalidChunkIndex)
            m_submittedChunkIndices.emplace_back(chunkToSubmit);
        
        if (m_currentChunkIndex == InvalidChunkIndex)
        {
            m_pool.emplace_back(CreateChunk(alignedSize));
            m_currentChunkIndex = (uint32_t)m_pool.size() - 1;
        }

        check(m_currentChunkIndex < (uint32_t)m_pool.size());
        Chunk& chunk = m_pool[m_currentChunkIndex];

        uint64_t pointer = chunk.pointer;
        chunk.pointer += alignedSize;
        return TransferMemory(m_device, chunk.buffer, pointer, alignedSize);
    }

    void TransferMemoryPool::Submit(uint64_t submissionId)
    {
        if (m_currentChunkIndex != InvalidChunkIndex)
        {
            m_submittedChunkIndices.emplace_back(m_currentChunkIndex);
            m_currentChunkIndex = InvalidChunkIndex;
        }

        for (uint32_t i = 0; i < (uint32_t)m_submittedChunkIndices.size(); ++i)
        {
            uint32_t index = m_submittedChunkIndices[i];
            if (m_pool[index].version == UINT64_MAX)
                m_pool[index].version = submissionId;
        }
    }

    TransferMemoryPool::Chunk TransferMemoryPool::CreateChunk(uint64_t size)
    {
        uint64_t alignedSize = utils::Align(size, Alignment);
        BufferDescription bufferDesc;
        bufferDesc.bufferUsage = BufferUsage_TransferSrc;
        bufferDesc.memoryUsage = MemoryUsage_CpuToGpu;
        bufferDesc.size = __max(alignedSize, m_defaultChunkSize);
        Chunk c;
        c.buffer = m_device->CreateBuffer(bufferDesc);
        return c;
    }

    CommandList::CommandList(Device* device)
        : m_device(device),
        m_currentCommandBuffer(nullptr),
        m_transferMemoryPool(m_device, 1<<16),
        m_dirtyBindings(true)
    {
        check(m_device);
    }

    CommandList::~CommandList()
    {
        check(!IsRecording());
        m_device->DestroyCommandList(this);
    }

    uint64_t CommandList::ExecuteCommandLists(CommandList* const* lists, uint32_t count)
    {
        static constexpr uint32_t MaxLists = 8;
        check(count <= MaxLists && count > 0 && lists);

        QueueType type = lists[0]->m_currentCommandBuffer->type;
        Mist::tStaticArray<CommandBuffer*, MaxLists> buffers;
        for (uint32_t i = 0; i < count; ++i)
        {
            check(lists[i] && lists[i]->m_currentCommandBuffer && lists[i]->m_currentCommandBuffer->type == type);
            buffers.Push(lists[i]->m_currentCommandBuffer);
        }

        CommandQueue* queue = lists[0]->m_device->GetCommandQueue(type);
        uint64_t submissionId = queue->SubmitCommandBuffers(buffers.GetData(), buffers.GetSize());

        for (uint32_t i = 0; i < count; ++i)
            lists[i]->Submitted(submissionId);

        return submissionId;
    }

    uint64_t CommandList::ExecuteCommandList()
    {
        CommandList* list = this;
        return ExecuteCommandLists(&list, 1);
    }

    void CommandList::BeginRecording()
    {
        check(!IsRecording());
        CommandQueue* queue = m_device->GetCommandQueue(Queue_Graphics | Queue_Compute | Queue_Transfer);
        m_currentCommandBuffer = queue->CreateCommandBuffer();
        m_currentCommandBuffer->Begin();
    }

    void CommandList::EndRecording()
    {
        check(IsRecording());
        EndRenderPass();
        FlushRequiredStates();
        m_currentCommandBuffer->End();
    }

    void CommandList::BeginMarker(const char* name, Color color)
    {
		check(m_device && IsRecording());
        const VulkanContext& context = m_device->GetContext();
        check(context.pfn_vkCmdBeginDebugUtilsLabelEXT);
        VkDebugUtilsLabelEXT label{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pNext = nullptr };
        label.color[0] = color.r;
        label.color[1] = color.g;
        label.color[2] = color.b;
        label.color[3] = color.a;
        label.pLabelName = name;
        context.pfn_vkCmdBeginDebugUtilsLabelEXT(m_currentCommandBuffer->cmd, &label);
    }

    void CommandList::EndMarker()
    {
		check(m_device && IsRecording());
		const VulkanContext& context = m_device->GetContext();
		check(context.pfn_vkCmdEndDebugUtilsLabelEXT);
        context.pfn_vkCmdEndDebugUtilsLabelEXT(m_currentCommandBuffer->cmd);
    }

    void CommandList::BindSets(const BindingSetVector& setsToBind, const BindingSetVector& currentBinding)
    {
        check(IsRecording());

        class BindingQueue
        {
        public:
            BindingQueue(CommandList* _cmd) : m_cmd(_cmd) {}
            ~BindingQueue()
            {
                FlushStoredBindings();
            }

            void FlushStoredBindings()
            {
                if (!m_vksets.IsEmpty())
                {
                    // flush sets
					check(m_firstSet != UINT32_MAX);
					m_cmd->FlushRequiredStates();
					VkPipelineBindPoint bindPoint = m_cmd->m_graphicsState.pipeline ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;
					VkPipelineLayout layout = m_cmd->m_graphicsState.pipeline ? m_cmd->m_graphicsState.pipeline->m_pipelineLayout : m_cmd->m_computeState.pipeline->m_pipelineLayout;
					vkCmdBindDescriptorSets(m_cmd->m_currentCommandBuffer->cmd, bindPoint, layout, m_firstSet, m_vksets.GetSize(), m_vksets.GetData(), m_offsets.GetSize(), m_offsets.GetData());

                    // clear queue
                    m_vksets.Clear();
                    m_offsets.Clear();
                    m_firstSet = UINT32_MAX;
                    m_lastSetStored = UINT32_MAX;
                }
            }

            void Push(uint32_t setIndex, VkDescriptorSet set, const uint32_t* offsets, uint32_t offsetCount)
            {
                if (m_lastSetStored != UINT32_MAX && setIndex - m_lastSetStored > 1)
                {
                    FlushStoredBindings();
                }
                m_lastSetStored = setIndex;

                if (m_firstSet == UINT32_MAX)
                    m_firstSet = setIndex;

                m_vksets.Push(set);
                for (uint32_t i = 0; i < offsetCount; ++i)
                    m_offsets.Push(offsets[i]);
            }

        private:
            CommandList* m_cmd;
			Mist::tStaticArray<VkDescriptorSet, BindingSetVector::MaxSets> m_vksets;
			Mist::tStaticArray<uint32_t, BindingSetVector::MaxDynamicsPerSet> m_offsets;
            uint32_t m_firstSet = UINT32_MAX;
            uint32_t m_lastSetStored = UINT32_MAX;
        } queue(this);

        for (uint32_t i = 0; i < BindingSetVector::MaxSets; ++i)
        {
            if (setsToBind.m_slots[i].set != currentBinding.m_slots[i].set ||
                !utils::EqualArrays(setsToBind.m_slots[i].offsets.GetData(), setsToBind.m_slots[i].offsets.GetSize(), currentBinding.m_slots[i].offsets.GetData(), currentBinding.m_slots[i].offsets.GetSize()))
            {
                if (setsToBind.m_slots[i].set)
                {
                    queue.Push(i, setsToBind.m_slots[i].set->m_set, setsToBind.m_slots[i].offsets.GetData(), setsToBind.m_slots[i].offsets.GetSize());

                    // set required texture states
                    for (uint32_t textureIndex = 0; textureIndex < (uint32_t)setsToBind.m_slots[i].set->m_textures.size(); ++textureIndex)
                    {
                        TextureBarrier barrier;
                        barrier.texture = setsToBind.m_slots[i].set->m_textures[textureIndex];
                        barrier.newLayout = render::ImageLayout_ShaderReadOnly;
                        //barrier.newLayout = utils::IsDepthStencilFormat(barrier.texture->m_description.format) ? render::ImageLayout_DepthStencilReadOnly : render::ImageLayout_ShaderReadOnly;
                        barrier.subresources = { 0,1,0,1 };
                        RequireTextureState(barrier);
                    }
                }
            }
		}
		queue.FlushStoredBindings();
    }

    void CommandList::SetGraphicsState(const GraphicsState& state)
    {
        PROF_ZONE_SCOPED("SetGraphicsState");
        check(AllowsCommandType(Queue_Graphics));

        // End render pass at the beginning, just in case there are some texture required states pending to flush.
        if (m_graphicsState.rt != state.rt)
            EndRenderPass();

        if (m_graphicsState.pipeline != state.pipeline && state.pipeline)
        {
            state.pipeline->UsePipeline(m_currentCommandBuffer);
            m_graphicsState.pipeline = state.pipeline;
            ++m_stats.pipelines;
        }

        if (state.vertexBuffer)
            BindVertexBuffer(state.vertexBuffer);
        if (state.indexBuffer)
            BindIndexBuffer(state.indexBuffer);

        BindSets(state.bindings, m_graphicsState.bindings);
        m_graphicsState.bindings = state.bindings;

        if (!m_graphicsState.rt && state.rt)
            BeginRenderPass(state.rt);
        m_graphicsState = state;
    }

    void CommandList::ClearColor(float r, float g, float b, float a) 
    {
        check(AllowsCommandType(Queue_Graphics));
        if (m_graphicsState.rt)
            m_graphicsState.rt->ClearColor(m_currentCommandBuffer, r, g, b, a);
    }

    void CommandList::ClearDepthStencil(float depth, uint32_t stencil) 
    {
        check(AllowsCommandType(Queue_Graphics));
        if (m_graphicsState.rt)
            m_graphicsState.rt->ClearDepthStencil(m_currentCommandBuffer, depth, stencil);
    }

    void CommandList::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) 
    {
        check(AllowsCommandType(Queue_Graphics));
        VkViewport viewport;
        viewport.x = x;
        viewport.y = y;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = minDepth;
        viewport.maxDepth = maxDepth;
        vkCmdSetViewport(m_currentCommandBuffer->cmd, 0, 1, &viewport);
    }

    void CommandList::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) 
    {
        check(AllowsCommandType(Queue_Graphics));
        VkRect2D rect{ .offset {x, y}, .extent{width, height} };
        vkCmdSetScissor(m_currentCommandBuffer->cmd, 0, 1, &rect);
    }

    void CommandList::BindVertexBuffer(BufferHandle vb)
    {
        check(AllowsCommandType(Queue_Graphics));
        if (vb == m_graphicsState.vertexBuffer)
            return;
        check(vb && vb->IsAllocated() && vb->m_description.bufferUsage & BufferUsage_VertexBuffer);
        m_graphicsState.vertexBuffer = vb;
        size_t offsets = 0;
        vkCmdBindVertexBuffers(m_currentCommandBuffer->cmd, 0, 1, &vb->m_buffer, &offsets);
    }

    void CommandList::BindIndexBuffer(BufferHandle ib)
    {
        check(AllowsCommandType(Queue_Graphics));
        if (ib == m_graphicsState.indexBuffer)
            return;
        check(ib && ib->IsAllocated() && ib->m_description.bufferUsage & BufferUsage_IndexBuffer);
        m_graphicsState.vertexBuffer = ib;
        size_t offsets = 0;
        vkCmdBindIndexBuffer(m_currentCommandBuffer->cmd, ib->m_buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        check(AllowsCommandType(Queue_Graphics));
        vkCmdDraw(m_currentCommandBuffer->cmd, vertexCount, instanceCount, firstVertex, firstInstance);
        check((vertexCount % 3) == 0);
        ++m_stats.drawCalls;
        m_stats.tris += vertexCount / 3;
    }

    void CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
    {
        check(AllowsCommandType(Queue_Graphics));
        vkCmdDrawIndexed(m_currentCommandBuffer->cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        check((indexCount % 3) == 0);
        ++m_stats.drawCalls;
        m_stats.tris += indexCount / 3;
    }

    void CommandList::SetComputeState(const ComputeState& state)
    {
        check(AllowsCommandType(Queue_Compute));
        if (state == m_computeState)
            return;

        EndRenderPass();
        m_computeState = state;
    }

    void CommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        check(AllowsCommandType(Queue_Compute));
        vkCmdDispatch(m_currentCommandBuffer->cmd, groupCountX, groupCountY, groupCountZ);
    }

    void CommandList::SetTextureState(const TextureBarrier* barriers, uint32_t count)
    {
        static constexpr uint32_t MaxBarriers = 16;
        check(count <= MaxBarriers);
        Mist::tStaticArray<VkImageMemoryBarrier2, MaxBarriers> imageBarriers;
        for (uint32_t i = 0; i < count; ++i)
        {
            //TextureSubresourceRange range = barriers[i].subresources.Resolve(barriers[i].texture->m_description);
            TextureSubresourceRange range = barriers[i].subresources;
            check(barriers[i].texture->m_layouts.contains(range));
            if (barriers[i].texture->m_layouts.at(range) != barriers[i].newLayout)
            {
                check(barriers[i].newLayout != ImageLayout_Undefined);
                imageBarriers.Push(utils::ConvertImageBarrier(barriers[i]));
                barriers[i].texture->m_layouts[range] = barriers[i].newLayout;
            }
        }
        if (!imageBarriers.IsEmpty())
        {
            // Pipeline barriers can't be done inside a render pass.
			check(!IsInsideRenderPass());
            VkDependencyInfo depInfo = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
            depInfo.imageMemoryBarrierCount = imageBarriers.GetSize();
            depInfo.pImageMemoryBarriers = imageBarriers.GetData();
            vkCmdPipelineBarrier2(m_currentCommandBuffer->cmd, &depInfo);
        }
    }

    void CommandList::SetTextureState(const TextureBarrier& barrier)
    {
        SetTextureState(&barrier, 1);
    }

    void CommandList::RequireTextureState(const TextureBarrier& barrier)
    {
        m_requiredStates.push_back(barrier);
    }

    void CommandList::WriteBuffer(BufferHandle buffer, const void* data, size_t size, size_t srcOffset, size_t dstOffset)
    {
        if (!data || !size || !buffer)
            return;

        check(buffer->m_description.size >= size + dstOffset);

        if (buffer->m_description.memoryUsage == MemoryUsage_CpuToGpu)
        {
            // dont need transfer command... (?)
            m_device->WriteBuffer(buffer, data, size, srcOffset, dstOffset);
        }
        else
        {
            // need transfer command, check buffer usage
            check(buffer->m_description.bufferUsage & BufferUsage_TransferDst);
            TransferMemory transferMemory = m_transferMemoryPool.Suballocate(size);
            transferMemory.MapMemory();
            transferMemory.Write(data, size, srcOffset);
            transferMemory.UnmapMemory();

            VkBufferCopy copyBuffer;
            copyBuffer.dstOffset = dstOffset;
            copyBuffer.srcOffset = transferMemory.m_pointer;
            copyBuffer.size = size;
            vkCmdCopyBuffer(m_currentCommandBuffer->cmd, transferMemory.m_buffer->m_buffer, buffer->m_buffer, 1, &copyBuffer);
        }
    }

    void CommandList::WriteTexture(TextureHandle texture, uint32_t mipLevel, uint32_t layer, const void* data, size_t dataSize)
    {
        FlushRequiredStates();

        const TextureDescription& description = texture->m_description;
        check(description.layers > layer && description.mipLevels > mipLevel);
        uint32_t mipWidth, mipHeight, mipDepth;
        utils::ComputeMipExtent(mipLevel, description.extent.width, description.extent.height, description.extent.depth, &mipWidth, &mipHeight, &mipDepth);

        size_t bytesBlock = utils::GetBytesPerPixel(description.format);
        size_t deviceSize = bytesBlock * size_t(mipWidth) * size_t(mipHeight) * size_t(mipDepth);
        check(deviceSize >= dataSize);

        TransferMemory transferMemory = m_transferMemoryPool.Suballocate(deviceSize);
        transferMemory.MapMemory();
        transferMemory.Write(data, dataSize, 0);
        transferMemory.UnmapMemory();

        TextureBarrier barrier;
        barrier.texture = texture;
        barrier.newLayout = ImageLayout_TransferDst;
        barrier.subresources.baseLayer = layer;
        barrier.subresources.countLayers = 1;
        barrier.subresources.baseMipLevel = mipLevel;
        barrier.subresources.countMipLevels = 1;
        ImageLayout oldLayout = texture->m_layouts.at(barrier.subresources);
        SetTextureState(barrier);

        VkBufferImageCopy copyRegion
        {
            .bufferOffset = transferMemory.m_pointer,
            .bufferRowLength = 0,
            .bufferImageHeight = 0
        };
        copyRegion.imageSubresource.aspectMask = utils::ConvertImageAspectFlags(description.format);
        copyRegion.imageSubresource.mipLevel = mipLevel;
        copyRegion.imageSubresource.baseArrayLayer = layer;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = { mipWidth, mipHeight, mipDepth };

        // Copy from temporal stage buffer to image
        vkCmdCopyBufferToImage(m_currentCommandBuffer->cmd,
            transferMemory.m_buffer->m_buffer, texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion);

        if (oldLayout != ImageLayout_Undefined)
        {
            barrier.newLayout = oldLayout;
            SetTextureState(barrier);
        }
    }

    void CommandList::BlitTexture(const BlitDescription& desc)
    {
        check(desc.src && desc.dst);

        TextureBarrier barriers[2];
        barriers[0].texture = desc.src;
        barriers[0].newLayout = ImageLayout_TransferSrc;
        barriers[0].subresources.baseLayer = desc.srcSubresource.layer;
        barriers[0].subresources.baseMipLevel = desc.srcSubresource.mipLevel;
        barriers[0].subresources.countMipLevels = 1;
        barriers[0].subresources.countLayers = desc.srcSubresource.layerCount;
        barriers[1].texture = desc.dst;
        barriers[1].newLayout = ImageLayout_TransferDst;
        barriers[1].subresources.baseLayer = desc.dstSubresource.layer;
        barriers[1].subresources.baseMipLevel = desc.dstSubresource.mipLevel;
        barriers[1].subresources.countMipLevels = 1;
        barriers[1].subresources.countLayers = desc.dstSubresource.layerCount;
        ImageLayout oldLayouts[2] = { desc.src->m_layouts.at(barriers[0].subresources), desc.dst->m_layouts.at(barriers[1].subresources) };
        SetTextureState(barriers, 2);

        VkImageBlit blit;
        memcpy_s(blit.srcOffsets, sizeof(Offset3D) * 2, desc.srcOffsets, sizeof(Offset3D) * 2);
        memcpy_s(blit.dstOffsets, sizeof(Offset3D) * 2, desc.dstOffsets, sizeof(Offset3D) * 2);
        TextureSubresourceLayer subresource = desc.srcSubresource.Resolve(desc.src->m_description);
        blit.srcSubresource = utils::ConvertImageSubresourceLayer(subresource, desc.src->m_description.format);
        subresource = desc.dstSubresource.Resolve(desc.dst->m_description);
        blit.dstSubresource = utils::ConvertImageSubresourceLayer(subresource, desc.dst->m_description.format);
        vkCmdBlitImage(m_currentCommandBuffer->cmd, desc.src->m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            desc.dst->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
            utils::ConvertFilter(desc.filter));

        //if (oldLayouts[0] != ImageLayout_Undefined)
        //    barriers[0].newLayout = oldLayouts[0];
        //if (oldLayouts[1] != ImageLayout_Undefined)
        //    barriers[1].newLayout = oldLayouts[1];
        //SetTextureState(barriers, 2);
    }

    void CommandList::BeginRenderPass(render::RenderTargetHandle rt)
    {
        if (!m_graphicsState.rt)
        {
            check(AllowsCommandType(Queue_Graphics));
            ++m_stats.rts;
            for (uint32_t i = 0; i < rt->m_description.colorAttachments.GetSize(); ++i)
                m_requiredStates.push_back(TextureBarrier{ rt->m_description.colorAttachments[i].texture, render::ImageLayout_ColorAttachment });
            if (rt->m_description.depthStencilAttachment.IsValid())
                m_requiredStates.push_back(TextureBarrier{ rt->m_description.depthStencilAttachment.texture, render::ImageLayout_DepthStencilAttachment });
            FlushRequiredStates();
            rt->BeginPass(m_currentCommandBuffer);
            m_graphicsState.rt = rt;
        }
    }

    void CommandList::EndRenderPass()
    {
        if (m_graphicsState.rt)
        {
            check(AllowsCommandType(Queue_Graphics));
            m_graphicsState.rt->EndPass(m_currentCommandBuffer);
            m_graphicsState.rt = nullptr;
        }
    }

    void CommandList::ClearState()
    {
        EndRenderPass();
        m_graphicsState = {};
        m_computeState = {};
    }

    void CommandList::Submitted(uint64_t submissionId)
    {
        m_currentCommandBuffer = nullptr;
        m_transferMemoryPool.Submit(submissionId);
    }

    void CommandList::FlushRequiredStates()
    {
        SetTextureState(m_requiredStates.data(), (uint32_t)m_requiredStates.size());
        m_requiredStates.clear();
    }

    Device::Device(const DeviceDescription& description)
        : m_context(nullptr), m_swapchainIndex(UINT32_MAX), m_queue(nullptr)
    {
        InitContext(description);
        InitMemoryContext();
        InitQueue();

        uint32_t backbufferWidth, backbufferHeight;
        Mist::Window::GetWindowExtent(description.windowHandle, backbufferWidth, backbufferHeight);
        InitSwapchain(backbufferWidth, backbufferHeight);
    }

    Device::~Device()
    {
        DestroySwapchain();
        DestroyQueue();
        DestroyMemoryContext();
        DestroyContext();
    }

    CommandQueue* Device::GetCommandQueue(QueueType type)
    {
        check(m_queue && ((m_queue->GetQueueType() & type) == type));
        return m_queue;
    }

    uint64_t Device::AlignUniformSize(uint64_t size) const
    {
        return utils::Align(size, m_context->GetMinUniformBufferOffsetAlignment());
    }

    CommandListHandle Device::CreateCommandList()
    {
        CommandList* cmd = _new CommandList(this);
        return CommandListHandle(cmd);
    }

    void Device::DestroyCommandList(CommandList* commandList)
    {
        // TODO check or wait for finishing pending transfer/graphics/compute operations of this command list.
        // the command list may have the last ref ptr to resources in use.
    }

    SemaphoreHandle Device::CreateRenderSemaphore(bool timelineSemaphore)
    {
        Semaphore* semaphore = _new Semaphore(this);

        VkSemaphoreTypeCreateInfo typeInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr };
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue = 0;

        VkSemaphoreCreateInfo info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = timelineSemaphore ? &typeInfo : nullptr };
        info.flags = 0;
        vkcheck(vkCreateSemaphore(m_context->device, &info, m_context->allocationCallbacks, &semaphore->m_semaphore));
        semaphore->m_isTimeline = timelineSemaphore;
        return SemaphoreHandle(semaphore);
    }

    void Device::DestroyRenderSemaphore(Semaphore* semaphore)
    {
        vkDestroySemaphore(m_context->device, semaphore->m_semaphore, m_context->allocationCallbacks);
    }

    uint64_t Device::GetSemaphoreTimelineCounter(SemaphoreHandle semaphore)
    {
        uint64_t v;
        vkcheck(vkGetSemaphoreCounterValue(m_context->device, semaphore->m_semaphore, &v));
        return v;
    }

    bool Device::WaitSemaphore(SemaphoreHandle semaphore, uint64_t timeoutNs)
    {
        VkSemaphoreWaitInfo info{};
        return false;
    }

    BufferHandle Device::CreateBuffer(const BufferDescription& description)
    {
        Buffer* buffer = _new Buffer(this);
        buffer->m_description = description;
        buffer->m_description.bufferUsage = utils::GetBufferUsage(description);
        check(description.size > 0);

        VkBufferUsageFlags usage = utils::ConvertBufferUsage(buffer->m_description.bufferUsage);
        VkBufferCreateInfo bufferInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = description.size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VmaAllocationCreateInfo allocInfo
        {
            .usage = utils::ConvertMemoryUsage(description.memoryUsage)
        };
        vkcheck(vmaCreateBuffer(m_context->memoryContext.allocator, &bufferInfo, &allocInfo,
            &buffer->m_buffer, &buffer->m_alloc, nullptr));
        SetDebugName(buffer, description.debugName.c_str());

        ++m_context->memoryContext.bufferStats.allocationCounts;
        m_context->memoryContext.bufferStats.currentAllocated += description.size;
        m_context->memoryContext.bufferStats.maxAllocated = __max(m_context->memoryContext.bufferStats.currentAllocated, m_context->memoryContext.bufferStats.maxAllocated);
        m_bufferTracking.push_back(buffer);
        return BufferHandle(buffer);
    }

    void Device::DestroyBuffer(Buffer* buffer)
    {
        check(m_context && buffer);
        --m_context->memoryContext.bufferStats.allocationCounts;
        check(m_context->memoryContext.bufferStats.currentAllocated - buffer->m_description.size < m_context->memoryContext.bufferStats.currentAllocated);
        m_context->memoryContext.bufferStats.currentAllocated -= buffer->m_description.size;

        vmaDestroyBuffer(m_context->memoryContext.allocator, buffer->m_buffer, buffer->m_alloc);
    }

    void Device::WriteBuffer(BufferHandle buffer, const void* data, size_t size, size_t srcOffset, size_t dstOffset)
    {
        check(dstOffset + size <= buffer->m_description.size);
        void* bufferPtr;
        vkcheck(vmaMapMemory(m_context->memoryContext.allocator, buffer->m_alloc, &bufferPtr));
        void* dst = reinterpret_cast<char*>(bufferPtr) + dstOffset;
        const void* src = reinterpret_cast<const char*>(data) + srcOffset;
        memcpy_s(dst, buffer->m_description.size-dstOffset, src, size);
        vmaUnmapMemory(m_context->memoryContext.allocator, buffer->m_alloc);
    }

    TextureHandle Device::CreateTexture(const TextureDescription& description)
    {
        check(description.format != Format_Undefined);
        check(description.extent.width != 0);
        check(description.extent.height != 0);
        check(description.extent.depth != 0);
        Texture* texture = _new Texture(this);
        texture->m_description = description;


        VmaAllocationCreateInfo allocInfo
        {
            .usage = utils::ConvertMemoryUsage(description.memoryUsage),
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

		for (uint32_t i = 0; i < description.layers; ++i)
		{
			for (uint32_t mip = 0; mip < description.mipLevels; ++mip)
			{
				TextureSubresourceRange range;
				range.baseLayer = i;
				range.countLayers = 1;
				range.baseMipLevel = mip;
				range.countMipLevels = 1;
				texture->m_layouts[range] = ImageLayout_Undefined;
			}
		}

        VkImageCreateInfo imageInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = nullptr };
        imageInfo.imageType = utils::ConvertImageType(description.dimension);
        imageInfo.flags = description.dimension == ImageDimension_Cube || description.dimension == ImageDimension_CubeArray ?
            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
        imageInfo.format = utils::ConvertFormat(description.format);
        imageInfo.extent.width = description.extent.width;
        imageInfo.extent.height = description.extent.height;
        imageInfo.extent.depth = description.extent.depth;
        imageInfo.mipLevels = description.mipLevels;
        imageInfo.arrayLayers = description.layers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = utils::ConvertImageUsage(utils::GetImageUsage(texture->m_description));
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = utils::ConvertImageLayout(ImageLayout_Undefined);
        imageInfo.pQueueFamilyIndices = nullptr;
        imageInfo.queueFamilyIndexCount = 0;

        // check format supported
        VkImageFormatProperties imageProperties;
        VkResult infoSupported = vkGetPhysicalDeviceImageFormatProperties(m_context->physicalDevice, 
            imageInfo.format, 
            imageInfo.imageType, 
            imageInfo.tiling, 
            imageInfo.usage, 
            imageInfo.flags,
            &imageProperties);
        if (infoSupported != VK_SUCCESS)
        {
            logferror("Image info not supported. Code result: %s\n", Mist::VkResultToStr(infoSupported));
            check(false && "Image information not supported.");
        }

        vkcheck(vmaCreateImage(m_context->memoryContext.allocator, &imageInfo, &allocInfo,
            &texture->m_image, &texture->m_alloc, nullptr));
        SetDebugName(texture, description.debugName.c_str());

        ++m_context->memoryContext.imageStats.allocationCounts;
        m_context->memoryContext.imageStats.currentAllocated += texture->GetImageSize();
        m_context->memoryContext.imageStats.maxAllocated = __max(m_context->memoryContext.imageStats.currentAllocated, m_context->memoryContext.imageStats.maxAllocated);
        m_textureTracking.push_back(texture);
        return TextureHandle(texture);
    }

    TextureHandle Device::CreateTextureFromNative(const TextureDescription& description, VkImage image)
    {
        check(image != VK_NULL_HANDLE);
        Texture* texture = _new Texture(this);
        texture->m_description = description;
        texture->m_alloc = nullptr;
        texture->m_image = image;
        texture->m_owner = false;
        for (uint32_t i = 0; i < description.layers; ++i)
        {
            for (uint32_t mip = 0; mip < description.mipLevels; ++mip)
            {
                TextureSubresourceRange range;
                range.baseLayer = i;
                range.countLayers = 1;
                range.baseMipLevel = mip;
                range.countMipLevels = 1;
                texture->m_layouts[range] = ImageLayout_Undefined;
            }
        }
        SetDebugName(texture, description.debugName.c_str());
        m_textureTracking.push_back(texture);
        return TextureHandle(texture);
    }

    void Device::DestroyTexture(Texture* texture)
    {
        check(m_context && texture);

        for (Texture::ViewIterator it = texture->m_views.begin(); it != texture->m_views.end(); ++it)
        {
            TextureView& view = it->second;
            vkDestroyImageView(m_context->device, view.m_view, m_context->allocationCallbacks);
            view.m_view = VK_NULL_HANDLE;
            view.m_image = VK_NULL_HANDLE;
        }

        if (texture->m_owner)
        {
            --m_context->memoryContext.imageStats.allocationCounts;
            check(m_context->memoryContext.imageStats.currentAllocated - texture->GetImageSize() < m_context->memoryContext.imageStats.currentAllocated);
            m_context->memoryContext.imageStats.currentAllocated -= texture->GetImageSize();
            vmaDestroyImage(m_context->memoryContext.allocator, texture->m_image, texture->m_alloc);
        }
    }

    SamplerHandle Device::CreateSampler(const SamplerDescription& description)
    {
        Sampler* sampler = _new Sampler(this);
        sampler->m_description = description;

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = nullptr;
        samplerInfo.magFilter = utils::ConvertFilter(description.magFilter);
        samplerInfo.minFilter = utils::ConvertFilter(description.minFilter);
        samplerInfo.addressModeU = utils::ConvertSamplerAddressMode(description.addressModeU);
        samplerInfo.addressModeV = utils::ConvertSamplerAddressMode(description.addressModeV);
        samplerInfo.addressModeW = utils::ConvertSamplerAddressMode(description.addressModeW);
        samplerInfo.anisotropyEnable = description.maxAnisotropy > 1.f ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = description.maxAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.flags = 0;
        samplerInfo.minLod = description.minLod == FLT_MAX ? 0.f : description.minLod;
        samplerInfo.maxLod = description.maxLod == FLT_MAX ? VK_LOD_CLAMP_NONE : description.maxLod;

        vkcheck(vkCreateSampler(m_context->device, &samplerInfo, m_context->allocationCallbacks, &sampler->m_sampler));
        SetDebugName(sampler, description.debugName.c_str());
        return SamplerHandle(sampler);
    }

    void Device::DestroySampler(Sampler* sampler)
    {
        check(m_context && sampler);
        vkDestroySampler(m_context->device, sampler->m_sampler, m_context->allocationCallbacks);
        sampler->m_sampler = VK_NULL_HANDLE;
    }

    ShaderHandle Device::CreateShader(const ShaderDescription& description, const void* binary, size_t binarySize)
    {
        Shader* shader = _new Shader(this);
        shader->m_description = description;
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.codeSize = binarySize*sizeof(uint32_t);
        createInfo.pCode = (const uint32_t*)binary;
        createInfo.flags = 0;
        vkcheck(vkCreateShaderModule(m_context->device, &createInfo, m_context->allocationCallbacks, &shader->m_shader));
        SetDebugName(shader, description.debugName.c_str());
        return ShaderHandle(shader);
    }

    void Device::DestroyShader(Shader* shader)
    {
        check(m_context && shader);
        vkDestroyShaderModule(m_context->device, shader->m_shader, m_context->allocationCallbacks);
        shader->m_shader = VK_NULL_HANDLE;
    }

    RenderTargetHandle Device::CreateRenderTarget(const RenderTargetDescription& description)
    {
        RenderTarget* renderTarget = _new RenderTarget(this);
        renderTarget->m_description = description;
        renderTarget->m_info = RenderTargetInfo(renderTarget->m_description);
        
        static constexpr uint32_t MaxAttachments = RenderTargetDescription::MaxRenderAttachments + 1;
        Mist::tStaticArray<VkAttachmentDescription, MaxAttachments> attachmentDescriptions(description.colorAttachments.GetSize());
        Mist::tStaticArray<VkAttachmentReference, MaxAttachments> colorReferences(description.colorAttachments.GetSize());
        VkAttachmentReference depthReference = {};

        Mist::tStaticArray<VkImageView, MaxAttachments> views;
        uint32_t numLayers = 0;

        for (uint32_t i = 0; i < description.colorAttachments.GetSize(); ++i)
        {
            const RenderTargetAttachment& rta = description.colorAttachments[i];
            check(rta.IsValid());
            TextureHandle texture = rta.texture;

            Format format = texture->m_description.format;
            check(format != Format_Undefined);

            VkAttachmentDescription& desc = attachmentDescriptions[i];
            desc.flags = 0;
            desc.format = utils::ConvertFormat(format);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference& ref = colorReferences[i];
            ref.attachment = i;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            TextureViewDescription viewDesc;
            viewDesc.format = format;
            viewDesc.dimension = texture->m_description.dimension;
            viewDesc.range = rta.range;
            TextureView* view = texture->GetView(viewDesc);
            views.Push(view->m_view);
        }

        if (description.depthStencilAttachment.IsValid())
        {
            const RenderTargetAttachment& rta = description.depthStencilAttachment;
            TextureHandle texture = rta.texture;

            Format format = texture->m_description.format;
            check(format != Format_Undefined && utils::IsDepthFormat(format));

            VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            if (rta.isReadOnly)
                layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            attachmentDescriptions.Push({});
            VkAttachmentDescription& desc = attachmentDescriptions.GetBack();
            desc.flags = 0;
            desc.format = utils::ConvertFormat(format);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            desc.initialLayout = layout;
            desc.finalLayout = layout;

            depthReference.attachment = attachmentDescriptions.GetSize() - 1;
            depthReference.layout = layout;

            TextureViewDescription viewDesc;
            viewDesc.format = format;
            viewDesc.dimension = texture->m_description.dimension;
            viewDesc.range = rta.range;
            TextureView* view = texture->GetView(viewDesc);
            views.Push(view->m_view);
        }

        check(views.GetSize() == attachmentDescriptions.GetSize());

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = (uint32_t)colorReferences.GetSize();
        subpass.pColorAttachments = colorReferences.GetData();
        subpass.pDepthStencilAttachment = description.depthStencilAttachment.IsValid() ? &depthReference : nullptr;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pNext = nullptr;
        renderPassInfo.attachmentCount = (uint32_t)attachmentDescriptions.GetSize();
        renderPassInfo.pAttachments = attachmentDescriptions.GetData();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 0;
        renderPassInfo.pDependencies = nullptr;
        vkcheck(vkCreateRenderPass(m_context->device, &renderPassInfo, m_context->allocationCallbacks, &renderTarget->m_renderPass));

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.pNext = nullptr;
        framebufferInfo.renderPass = renderTarget->m_renderPass;
        framebufferInfo.attachmentCount = (uint32_t)views.GetSize();
        framebufferInfo.pAttachments = views.GetData();
        framebufferInfo.width = renderTarget->m_info.extent.width;
        framebufferInfo.height = renderTarget->m_info.extent.height;
        framebufferInfo.layers = 1;
        vkcheck(vkCreateFramebuffer(m_context->device, &framebufferInfo, m_context->allocationCallbacks, &renderTarget->m_framebuffer));


        SetDebugName(renderTarget, description.debugName.c_str());
        return RenderTargetHandle(renderTarget);
    }

    void Device::DestroyRenderTarget(RenderTarget* renderTarget)
    {
        check(m_context && renderTarget);
        vkDestroyRenderPass(m_context->device, renderTarget->m_renderPass, m_context->allocationCallbacks);
        vkDestroyFramebuffer(m_context->device, renderTarget->m_framebuffer, m_context->allocationCallbacks);
        renderTarget->m_renderPass = VK_NULL_HANDLE;
        renderTarget->m_framebuffer = VK_NULL_HANDLE;
    }

    GraphicsPipelineHandle Device::CreateGraphicsPipeline(const GraphicsPipelineDescription& description, RenderTargetHandle rt)
    {
        GraphicsPipeline* pipeline = _new GraphicsPipeline(this);
        pipeline->m_description = description;

        check(rt && rt->m_renderPass != VK_NULL_HANDLE);

        // Pipeline Shader Stages
        ShaderHandle shaders[2] = { description.vertexShader, description.fragmentShader };
        VkPipelineShaderStageCreateInfo shaderStages[2];
        uint32_t stageCount = 0;
        for (uint32_t i = 0; i < Mist::CountOf(shaderStages); ++i)
        {
            if (!shaders[i])
                continue;
            check(shaders[i]->m_shader != VK_NULL_HANDLE);
            shaderStages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[stageCount].pNext = nullptr;
            shaderStages[stageCount].flags = 0;
            shaderStages[stageCount].stage = (VkShaderStageFlagBits)utils::ConvertShaderStage(shaders[i]->m_description.type);
            shaderStages[stageCount].module = shaders[i]->m_shader;
            shaderStages[stageCount].pName = "main";
            shaderStages[stageCount].pSpecializationInfo = nullptr;
            ++stageCount;
        }

        // Pipeline Input Layout
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.pNext = nullptr;
        vertexInputInfo.flags = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = description.vertexInputLayout.m_attributes.GetSize();
        vertexInputInfo.pVertexAttributeDescriptions = description.vertexInputLayout.m_attributes.GetData();
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &description.vertexInputLayout.m_binding;

        // Pipeline Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.pNext = nullptr;
        inputAssembly.flags = 0;
        inputAssembly.topology = utils::ConvertPrimitiveType(description.primitiveType);
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Pipeline Viewport
        VkViewport viewport = {};
        viewport.x = description.renderState.viewportState.viewport.x;
        viewport.y = description.renderState.viewportState.viewport.y;
        viewport.width = description.renderState.viewportState.viewport.width;
        viewport.height = description.renderState.viewportState.viewport.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        VkRect2D scissor = {};
        scissor.extent.width = (uint32_t)description.renderState.viewportState.scissor.GetWidth();
        scissor.extent.height = (uint32_t)description.renderState.viewportState.scissor.GetHeight();
        scissor.offset.x = (int32_t)description.renderState.viewportState.scissor.minX;
        scissor.offset.y = (int32_t)description.renderState.viewportState.scissor.minY;
        VkPipelineViewportStateCreateInfo viewportInfo = {};
        viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportInfo.pNext = nullptr;
        viewportInfo.flags = 0;
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = &viewport;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = &scissor;

        // Pipeline Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.pNext = nullptr;
        rasterizer.flags = 0;
        rasterizer.cullMode = utils::ConvertCullMode(description.renderState.rasterState.cullMode);
        rasterizer.frontFace = description.renderState.rasterState.frontFaceCounterClockWise 
            ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        rasterizer.polygonMode = utils::ConvertPolygonMode(description.renderState.rasterState.fillMode);
        rasterizer.depthClampEnable = description.renderState.rasterState.depthClampEnable ? VK_TRUE : VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.depthBiasEnable = description.renderState.rasterState.depthBiasEnable;
        rasterizer.depthBiasConstantFactor = description.renderState.rasterState.depthBiasConstantFactor;
        rasterizer.depthBiasClamp = description.renderState.rasterState.depthBiasClamp;
        rasterizer.depthBiasSlopeFactor = description.renderState.rasterState.depthBiasSlopeFactor;
        rasterizer.lineWidth = 1.f;

        // Pipeline Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.pNext = nullptr;
        multisampling.flags = 0;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.minSampleShading = 1.f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        // Pipeline Depth Stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.pNext = nullptr;
        depthStencil.flags = 0;
        depthStencil.minDepthBounds = 0.f;
        depthStencil.maxDepthBounds = 1.f;
        depthStencil.depthTestEnable = description.renderState.depthStencilState.depthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = description.renderState.depthStencilState.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = utils::ConvertCompareOp(description.renderState.depthStencilState.depthCompareOp);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = description.renderState.depthStencilState.stencilTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.front.failOp = utils::ConvertStencilOp(description.renderState.depthStencilState.frontFace.failOp);
        depthStencil.front.passOp = utils::ConvertStencilOp(description.renderState.depthStencilState.frontFace.passOp);
        depthStencil.front.depthFailOp = utils::ConvertStencilOp(description.renderState.depthStencilState.frontFace.depthFailOp);
        depthStencil.front.compareOp = utils::ConvertCompareOp(description.renderState.depthStencilState.frontFace.compareOp);
        depthStencil.front.compareMask = description.renderState.depthStencilState.stencilReadMask;
        depthStencil.front.writeMask = description.renderState.depthStencilState.stencilWriteMask;
        depthStencil.front.reference = description.renderState.depthStencilState.stencilRefValue;
        depthStencil.back = depthStencil.front;
        depthStencil.front.failOp = utils::ConvertStencilOp(description.renderState.depthStencilState.backFace.failOp);
        depthStencil.front.passOp = utils::ConvertStencilOp(description.renderState.depthStencilState.backFace.passOp);
        depthStencil.front.depthFailOp = utils::ConvertStencilOp(description.renderState.depthStencilState.backFace.depthFailOp);
        depthStencil.front.compareOp = utils::ConvertCompareOp(description.renderState.depthStencilState.backFace.compareOp);

        // Pipeline Color Blending
        Mist::tStaticArray<VkPipelineColorBlendAttachmentState, RenderTargetDescription::MaxRenderAttachments> colorBlendAttachments;
        for (uint32_t i = 0; i < description.renderState.blendState.renderTargetBlendStates.GetSize(); ++i)
        {
            const RenderTargetBlendState& blendState = description.renderState.blendState.renderTargetBlendStates[i];
            VkPipelineColorBlendAttachmentState& colorBlend = colorBlendAttachments.Push();
            colorBlend.blendEnable = blendState.blendEnable ? VK_TRUE : VK_FALSE;
            colorBlend.colorWriteMask = utils::ConvertColorComponentFlags(blendState.colorWriteMask);
            colorBlend.srcColorBlendFactor = utils::ConvertBlendFactor(blendState.srcBlend);
            colorBlend.dstColorBlendFactor = utils::ConvertBlendFactor(blendState.dstBlend);
            colorBlend.colorBlendOp = utils::ConvertBlendOp(blendState.blendOp);
            colorBlend.srcAlphaBlendFactor = utils::ConvertBlendFactor(blendState.srcAlphaBlend);
            colorBlend.dstAlphaBlendFactor = utils::ConvertBlendFactor(blendState.dstAlphaBlend);
            colorBlend.alphaBlendOp = utils::ConvertBlendOp(blendState.alphaBlendOp);
        }
        // check color blend attachments matches with render pass description
        check(colorBlendAttachments.GetSize() == rt->m_description.colorAttachments.GetSize());

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.pNext = nullptr;
        colorBlending.flags = 0;
        colorBlending.attachmentCount = colorBlendAttachments.GetSize();
        colorBlending.pAttachments = colorBlendAttachments.GetData();
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        
        // Pipeline Dynamic States
        // Set as default Viewport and Scissor
#if 0
        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            //VK_DYNAMIC_STATE_LINE_WIDTH,
            //VK_DYNAMIC_STATE_DEPTH_BIAS,
            //VK_DYNAMIC_STATE_BLEND_CONSTANTS,
            //VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
            //VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            //VK_DYNAMIC_STATE_STENCIL_REFERENCE
        };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext = nullptr;
        dynamicState.flags = 0;
        dynamicState.dynamicStateCount = Mist::CountOf(dynamicStates);
        dynamicState.pDynamicStates = dynamicStates;
#else
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext = nullptr;
        dynamicState.flags = 0;
        dynamicState.dynamicStateCount = 0;
        dynamicState.pDynamicStates = nullptr;
#endif // 0


        // create pipeline layout
        pipeline->m_pipelineLayout = VK_NULL_HANDLE;
        CreatePipelineLayout(this, description.bindingLayouts, pipeline->m_pipelineLayout);
        check(pipeline->m_pipelineLayout != VK_NULL_HANDLE);


        VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {};
        graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineInfo.pNext = nullptr;
        graphicsPipelineInfo.flags = 0;
        graphicsPipelineInfo.layout = pipeline->m_pipelineLayout;
        graphicsPipelineInfo.stageCount = stageCount;
        graphicsPipelineInfo.pStages = shaderStages;
        graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
        graphicsPipelineInfo.pInputAssemblyState = &inputAssembly;
        graphicsPipelineInfo.pTessellationState = nullptr;
        graphicsPipelineInfo.pViewportState = &viewportInfo;
        graphicsPipelineInfo.pRasterizationState = &rasterizer;
        graphicsPipelineInfo.pMultisampleState = &multisampling;
        graphicsPipelineInfo.pDepthStencilState = &depthStencil;
        graphicsPipelineInfo.pColorBlendState = &colorBlending;
        graphicsPipelineInfo.pDynamicState = &dynamicState;
        graphicsPipelineInfo.subpass = 0;
        graphicsPipelineInfo.renderPass = rt->m_renderPass;
        graphicsPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        graphicsPipelineInfo.basePipelineIndex = -1;

        vkcheck(vkCreateGraphicsPipelines(m_context->device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, m_context->allocationCallbacks, &pipeline->m_pipeline));
        SetDebugName(pipeline, description.debugName.c_str());
        return GraphicsPipelineHandle(pipeline);
    }

    void Device::DestroyGraphicsPipeline(GraphicsPipeline* pipeline)
    {
        check(m_context && pipeline);
        vkDestroyPipeline(m_context->device, pipeline->m_pipeline, m_context->allocationCallbacks);
        vkDestroyPipelineLayout(m_context->device, pipeline->m_pipelineLayout, m_context->allocationCallbacks);
    }

    BindingLayoutHandle Device::CreateBindingLayout(const BindingLayoutDescription& description)
    {
        BindingLayout* bindingLayout = _new BindingLayout(this);
        bindingLayout->m_description = description;

        // Create descriptor set layout
        Mist::tStaticArray<VkDescriptorSetLayoutBinding, BindingLayoutDescription::MaxBindings> bindings;

        for (uint32_t i = 0; i < description.bindings.GetSize(); ++i)
        {
            const BindingLayoutItem& binding = description.bindings[i];
            check(binding.arrayCount > 0);
            VkDescriptorSetLayoutBinding& b = bindings.Push();
            b.binding = binding.binding;
            b.descriptorType = utils::ConvertToDescriptorType(binding.type);
            b.descriptorCount = binding.arrayCount;
            b.stageFlags = utils::ConvertShaderStage(binding.shaderType);
            b.pImmutableSamplers = nullptr;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;
        layoutInfo.flags = 0;
        layoutInfo.bindingCount = bindings.GetSize();
        layoutInfo.pBindings = bindings.GetData();
        vkcheck(vkCreateDescriptorSetLayout(m_context->device, &layoutInfo, m_context->allocationCallbacks, &bindingLayout->m_layout));

        // Compute descriptor pool sizes
        auto findFn = [](const VkDescriptorPoolSize* data, uint32_t count, VkDescriptorType type) -> uint32_t
            {
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (data[i].type == type)
                        return i;
                }
                return UINT32_MAX;
            };

        for (uint32_t i = 0; i < bindings.GetSize(); ++i)
        {
            const VkDescriptorSetLayoutBinding& binding = bindings[i];
            uint32_t index = findFn(bindingLayout->m_poolSizes.GetData(), bindingLayout->m_poolSizes.GetSize(), binding.descriptorType);
            if (index == UINT32_MAX)
            {
                VkDescriptorPoolSize& poolSize = bindingLayout->m_poolSizes.Push();
                poolSize.type = binding.descriptorType;
                poolSize.descriptorCount = 0;
                index = bindingLayout->m_poolSizes.GetSize() - 1;
            }
            bindingLayout->m_poolSizes[index].descriptorCount += binding.descriptorCount;
        }

        SetDebugName(bindingLayout, description.debugName.c_str());
        return BindingLayoutHandle(bindingLayout);
    }

    void Device::DestroyBindingLayout(BindingLayout* bindingLayout)
    {
        check(m_context && bindingLayout);
        vkDestroyDescriptorSetLayout(m_context->device, bindingLayout->m_layout, m_context->allocationCallbacks);
    }

    ComputePipelineHandle Device::CreateComputePipeline(const ComputePipelineDescription& description)
    {
        ComputePipeline* pipeline = _new ComputePipeline(this);
        pipeline->m_description = description;
        
        check(description.computeShader && description.computeShader->m_shader != VK_NULL_HANDLE);

        VkPipelineShaderStageCreateInfo shaderStageInfo = {};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.pNext = nullptr;
        shaderStageInfo.flags = 0;
        shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStageInfo.module = description.computeShader->m_shader;
        shaderStageInfo.pName = "main";
        shaderStageInfo.pSpecializationInfo = nullptr;

        // Create pipeline layout
        pipeline->m_pipelineLayout = VK_NULL_HANDLE;
        CreatePipelineLayout(this, description.bindingLayouts, pipeline->m_pipelineLayout);
        check(pipeline->m_pipelineLayout != VK_NULL_HANDLE);

        VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = nullptr;
        pipelineInfo.flags = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.stage = shaderStageInfo;
        pipelineInfo.layout = pipeline->m_pipelineLayout;

        vkcheck(vkCreateComputePipelines(m_context->device, VK_NULL_HANDLE, 1, &pipelineInfo, m_context->allocationCallbacks, &pipeline->m_pipeline));
        SetDebugName(pipeline, description.debugName.c_str());
        return ComputePipelineHandle(pipeline);
    }

    void Device::DestroyComputePipeline(ComputePipeline* pipeline)
    {
        check(m_context && pipeline);
        vkDestroyPipeline(m_context->device, pipeline->m_pipeline, m_context->allocationCallbacks);
        vkDestroyPipelineLayout(m_context->device, pipeline->m_pipelineLayout, m_context->allocationCallbacks);
    }

    BindingSetHandle Device::CreateBindingSet(const BindingSetDescription& description, BindingLayoutHandle layout)
    {
        BindingSet* bindingSet = _new BindingSet(this);
        bindingSet->m_description = description;
        bindingSet->m_layout = layout;
        check(layout && layout->m_layout != VK_NULL_HANDLE);

        // Create pool
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.pNext = nullptr;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = layout->m_poolSizes.GetSize();
        poolInfo.pPoolSizes = layout->m_poolSizes.GetData();
        vkcheck(vkCreateDescriptorPool(m_context->device, &poolInfo, m_context->allocationCallbacks, &bindingSet->m_pool));

        // Allocate descriptors
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.descriptorPool = bindingSet->m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout->m_layout;
        vkcheck(vkAllocateDescriptorSets(m_context->device, &allocInfo, &bindingSet->m_set));

        // Update descriptors
        Mist::tStaticArray<VkWriteDescriptorSet, BindingSetItem::MaxBindingSets> writes;
        Mist::tStaticArray<VkDescriptorBufferInfo, BindingSetItem::MaxBindingSets> bufferInfos;
        Mist::tStaticArray<VkDescriptorImageInfo, BindingSetItem::MaxBindingSets> imageInfos;

        auto fillWriteDescriptorSet = [&](uint32_t binding, VkDescriptorType type,
            VkDescriptorImageInfo* imageInfo, VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount = 1)
            {
                VkWriteDescriptorSet& w = writes.Push();
                w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.pNext = nullptr;
                w.descriptorCount = descriptorCount;
                w.dstArrayElement = 0;
                w.descriptorType = type;
                w.dstSet = bindingSet->m_set;
                w.pBufferInfo = bufferInfo;
                w.pImageInfo = imageInfo;
                w.pTexelBufferView = nullptr;
                w.dstBinding = binding;
            };

        for (uint32_t i = 0; i < description.GetBindingItemCount(); ++i)
        {
            const BindingSetItem& item = description.bindingItems[i];
            check(item.binding < layout->m_description.bindings.GetSize());
            const BindingLayoutItem& layoutItem = layout->m_description.bindings[item.binding];

            check(item.buffer || !item.textures.IsEmpty());
            check(utils::ConvertToDescriptorType(item.type) == utils::ConvertToDescriptorType(layoutItem.type)
                && item.binding == layoutItem.binding);

            switch (item.type)
            {
            case ResourceType_TextureSRV:
            case ResourceType_TextureUAV:
            {
                check(item.type == ResourceType_TextureUAV || (item.samplers.GetSize() == item.textures.GetSize() && item.samplers.GetSize() == item.textureSubresources.GetSize()));

                uint32_t baseImageInfo = imageInfos.GetSize();
                for (uint32_t j = 0; j < item.textures.GetSize(); ++j)
                {
                    // cache resources
                    TextureHandle texture = item.textures[j];
                    SamplerHandle sampler = nullptr;
                    bindingSet->m_textures.push_back(texture);
                    if (item.type == ResourceType_TextureSRV)
                    {
                        sampler = item.samplers[j];
                        bindingSet->m_samplers.push_back(sampler);
                    }

                    // generate texture view
                    TextureViewDescription viewDescription;
                    viewDescription.dimension = item.dimension;
                    viewDescription.range = item.textureSubresources[j].Resolve(texture->m_description);
                    viewDescription.format = texture->m_description.format;
                    if (utils::IsDepthFormat(texture->m_description.format))
                        viewDescription.viewOnlyDepth = true;
                    TextureView* view = texture->GetView(viewDescription);
                    check(view && view->m_view != VK_NULL_HANDLE);

                    // generate image info for descriptor
                    VkDescriptorImageInfo& imageInfo = imageInfos.Push();
                    imageInfo.imageLayout = item.type == ResourceType_TextureSRV ?
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
                    imageInfo.imageView = view->m_view;
                    imageInfo.sampler = sampler ? sampler->m_sampler : VK_NULL_HANDLE;
                }
                fillWriteDescriptorSet(layoutItem.binding, utils::ConvertToDescriptorType(layoutItem.type), 
                    imageInfos.GetData() + baseImageInfo, nullptr, item.textures.GetSize());
            }
            break;
            case ResourceType_ConstantBuffer:
            case ResourceType_VolatileConstantBuffer:
            case ResourceType_BufferUAV:
            {
                BufferHandle buffer = item.buffer;
                bindingSet->m_buffers.push_back(buffer);
                BufferRange range = item.bufferRange.Resolve(buffer->m_description);

                VkDescriptorBufferInfo& bufferInfo = bufferInfos.Push();
                bufferInfo.buffer = buffer->m_buffer;
                bufferInfo.offset = range.offset;
                bufferInfo.range = range.size;

                fillWriteDescriptorSet(layoutItem.binding, utils::ConvertToDescriptorType(layoutItem.type), nullptr, &bufferInfo);
            }
            break;
            }
        }

        vkUpdateDescriptorSets(m_context->device, writes.GetSize(), writes.GetData(), 0, nullptr);
        SetDebugName(bindingSet, description.debugName.c_str());
        return BindingSetHandle(bindingSet);
    }

    void Device::DestroyBindingSet(BindingSet* bindingSet)
    {
        check(m_context && bindingSet);
        bindingSet->m_set = VK_NULL_HANDLE;
        vkDestroyDescriptorPool(m_context->device, bindingSet->m_pool, m_context->allocationCallbacks);
        bindingSet->m_buffers.clear();
        bindingSet->m_textures.clear();
        bindingSet->m_samplers.clear();
    }

    uint32_t Device::AcquireSwapchainIndex(SemaphoreHandle semaphoreToBeSignaled)
    {
        check(m_swapchainIndex == UINT32_MAX);
        vkcheck(vkAcquireNextImageKHR(m_context->device, m_swapchain.swapchain, 1000000000, semaphoreToBeSignaled ? semaphoreToBeSignaled->m_semaphore : nullptr, nullptr, &m_swapchainIndex));
        check(m_swapchainIndex < (uint32_t)m_swapchain.images.size());
        return m_swapchainIndex;
    }

    void Device::Present(SemaphoreHandle semaphoreToWait)
    {
        check(m_swapchainIndex < (uint32_t)m_swapchain.images.size());
        // Present
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &m_swapchain.swapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pWaitSemaphores = semaphoreToWait ? &semaphoreToWait->m_semaphore : nullptr;
        presentInfo.waitSemaphoreCount = semaphoreToWait ? 1 : 0;
        presentInfo.pImageIndices = &m_swapchainIndex;
        vkcheck(vkQueuePresentKHR(m_queue->m_queue, &presentInfo));
        m_swapchainIndex = UINT32_MAX;
    }

    void Device::WaitIdle()
    {
        vkcheck(vkDeviceWaitIdle(m_context->device));
    }

    bool Device::WaitForSubmissionId(uint64_t submissionId, uint64_t timeoutCounter) const
    {
        uint64_t c = 0;
        while (c++ < timeoutCounter && !m_queue->WaitForCommandSubmission(submissionId, 1000000000))
            logferror("[Device] Timeout wait for command submission (%d)\n", submissionId);
        return c <= timeoutCounter;
    }

    void Device::SetDebugName(Semaphore* object, const char* debugName) const
    {
        SetDebugName(object->m_semaphore, debugName, VK_OBJECT_TYPE_SEMAPHORE);
    }

    void Device::SetDebugName(Buffer* object, const char* debugName) const
    {
        SetDebugName(object->m_buffer, debugName, VK_OBJECT_TYPE_BUFFER);
    }

    void Device::SetDebugName(Texture* object, const char* debugName) const
    {
        SetDebugName(object->m_image, debugName, VK_OBJECT_TYPE_IMAGE);
        for (auto& it : object->m_views)
            SetDebugName(it.second.m_view, debugName, VK_OBJECT_TYPE_IMAGE_VIEW);
    }

    void Device::SetDebugName(Sampler* object, const char* debugName) const
    {
        SetDebugName(object->m_sampler, debugName, VK_OBJECT_TYPE_SAMPLER);
    }

    void Device::SetDebugName(Shader* object, const char* debugName) const
    {
        SetDebugName(object->m_shader, debugName, VK_OBJECT_TYPE_SHADER_MODULE);
    }

    void Device::SetDebugName(RenderTarget* object, const char* debugName) const
    {
        SetDebugName(object->m_renderPass, debugName, VK_OBJECT_TYPE_RENDER_PASS);
        SetDebugName(object->m_framebuffer, debugName, VK_OBJECT_TYPE_FRAMEBUFFER);
    }

    void Device::SetDebugName(GraphicsPipeline* object, const char* debugName) const
    {
        SetDebugName(object->m_pipeline, debugName, VK_OBJECT_TYPE_PIPELINE);
        SetDebugName(object->m_pipelineLayout, debugName, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
    }

    void Device::SetDebugName(BindingLayout* object, const char* debugName) const
    {
        SetDebugName(object->m_layout, debugName, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
    }

    void Device::SetDebugName(BindingSet* object, const char* debugName) const
    {
        SetDebugName(object->m_set, debugName, VK_OBJECT_TYPE_DESCRIPTOR_SET);
        SetDebugName(object->m_pool, debugName, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
    }

    void Device::SetDebugName(ComputePipeline* object, const char* debugName) const
    {
        SetDebugName(object->m_pipeline, debugName, VK_OBJECT_TYPE_PIPELINE);
        SetDebugName(object->m_pipelineLayout, debugName, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
    }

    void Device::SetDebugName(const void* object, const char* debugName, uint32_t type) const
    {
        VkDebugUtilsObjectNameInfoEXT info{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, .pNext = nullptr };
        info.objectType = (VkObjectType)type;
        info.objectHandle = *(const uint64_t*)(&object);
        info.pObjectName = debugName;
        vkcheck(m_context->pfn_vkSetDebugUtilsObjectNameEXT(m_context->device, &info));
    }

    void Device::InitContext(const DeviceDescription& description)
    {
        // Get Vulkan version
        uint32_t instanceVersion = VK_API_VERSION_1_0;
        PFN_vkEnumerateInstanceVersion FN_vkEnumerateInstanceVersion = PFN_vkEnumerateInstanceVersion(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
        if (FN_vkEnumerateInstanceVersion)
        {
            vkcheck(FN_vkEnumerateInstanceVersion(&instanceVersion));
        }
        else
            logerror("Fail to get Vulkan API version. Using base api version.\n");

        // 3 macros to extract version info
        uint32_t major = VK_VERSION_MAJOR(instanceVersion);
        uint32_t minor = VK_VERSION_MINOR(instanceVersion);
        uint32_t patch = VK_VERSION_PATCH(instanceVersion);
        logfok("Vulkan API version: %d.%d.%d\n", major, minor, patch);

        vkb::InstanceBuilder builder;
        vkb::Result<vkb::Instance> instanceReturn = builder
            .set_app_name("Vulkan renderer")
            .request_validation_layers(Mist::CVar_EnableValidationLayer.Get())
            .require_api_version(major, minor, patch)
            .set_debug_callback(&DebugVulkanCallback)
            //.enable_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)
            .build();
        check(instanceReturn.has_value());
        if (instanceReturn.matches_error(vkb::InstanceError::failed_create_instance))
            check(false && "Failed to create vulkan instance.\n");

        VkInstance instance = instanceReturn.value().instance;
        VkDebugUtilsMessengerEXT vkDebugMessenger = instanceReturn.value().debug_messenger;

        // Physical device
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        // TODO: app abstraction to separate window functionality
        Mist::Window::CreateSurface(*reinterpret_cast<const Mist::Window*>(description.windowHandle), &instance, &surface);
        vkb::PhysicalDeviceSelector selector(instanceReturn.value());
        vkb::PhysicalDevice vkbPhysicalDevice = selector
            .set_minimum_version(1, 1)
            .set_surface(surface)
            .allow_any_gpu_device_type(false)
            .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
            .select()
            .value();
        vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };

        // Enable shader draw parameters
        VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParamsFeatures = {};
        shaderDrawParamsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
        shaderDrawParamsFeatures.pNext = nullptr;
        shaderDrawParamsFeatures.shaderDrawParameters = VK_TRUE;
        deviceBuilder.add_pNext(&shaderDrawParamsFeatures);

        // Build PhysicalDeviceFeatures
        VkPhysicalDeviceFeatures2 features;
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = nullptr;
        vkGetPhysicalDeviceFeatures2(vkbPhysicalDevice.physical_device, &features);
        deviceBuilder.add_pNext(&features);

        // Enable timeline semaphores.
        VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures;
        timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        timelineSemaphoreFeatures.pNext = nullptr;
        timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;
        deviceBuilder.add_pNext(&timelineSemaphoreFeatures);

        // Enable synchronization2
        VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features;
        sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
        sync2Features.pNext = nullptr;
        sync2Features.synchronization2 = VK_TRUE;
        deviceBuilder.add_pNext(&sync2Features);

        // Create Device
        vkb::Result<vkb::Device> deviceResult = deviceBuilder.build();
        check(deviceResult.has_value());
        VkDevice device = deviceResult.value().device;
        VkPhysicalDevice physicalDevice = vkbPhysicalDevice.physical_device;

        // Compute queue from device
#if 0
        m_renderContext.ComputeQueue = device.get_queue(vkb::QueueType::compute).value();
        m_renderContext.ComputeQueueFamily = device.get_queue_index(vkb::QueueType::compute).value();
        check(m_renderContext.ComputeQueue != VK_NULL_HANDLE);
        check(m_renderContext.ComputeQueueFamily != (uint32_t)vkb::QueueError::invalid_queue_family_index
            && m_renderContext.ComputeQueueFamily != vkb::detail::QUEUE_INDEX_MAX_VALUE);

#elif 0
        // Get queue families and find a queue that supports both graphics and compute
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_renderContext.GPUDevice, &queueFamilyCount, nullptr);
        VkQueueFamilyProperties* queueFamilyProperties = _new VkQueueFamilyProperties[queueFamilyCount];
        vkGetPhysicalDeviceQueueFamilyProperties(m_renderContext.GPUDevice, &queueFamilyCount, queueFamilyProperties);

        uint32_t graphicsComputeQueueIndex = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            logfinfo("DeviceQueue %d:\n", i);
            logfinfo("* Graphics: %d, Compute: %d, Transfer: %d, SparseBinding: %d\n",
                queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ? 1 : 0,
                queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT ? 1 : 0,
                queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT ? 1 : 0,
                queueFamilyProperties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ? 1 : 0);

            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
                && queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                graphicsComputeQueueIndex = i;
            }
        }

        check(graphicsComputeQueueIndex != UINT32_MAX && "Architecture doesn't support separate graphics and compute hardware queues.");
        delete[] queueFamilyProperties;
        queueFamilyProperties = nullptr;

        m_renderContext.ComputeQueueFamily = graphicsComputeQueueIndex;
        vkGetDeviceQueue(m_renderContext.Device, graphicsComputeQueueIndex, 0, &m_renderContext.ComputeQueue);
        check(m_renderContext.ComputeQueue);
#endif // 0


        // Dump physical device info
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        logfinfo("Physical device:\n\t- %s\n\t- Id: %d\n\t- VendorId: %d\n",
            deviceProperties.deviceName, deviceProperties.deviceID, deviceProperties.vendorID);

        check(!m_context);
        m_context = _new VulkanContext(instance, device, surface, physicalDevice, vkDebugMessenger, nullptr);

        const VkPhysicalDeviceProperties& properties = m_context->physicalDeviceProperties;
        logfinfo("GPU has minimum buffer alignment of %Id bytes.\n",
            properties.limits.minUniformBufferOffsetAlignment);
        logfinfo("GPU max bound descriptor sets: %d\n",
            properties.limits.maxBoundDescriptorSets);
    }

    void Device::InitMemoryContext()
    {
        check(m_context && m_context->memoryContext.allocator == VK_NULL_HANDLE);
        m_context->memoryContext.imageStats = {};
        m_context->memoryContext.bufferStats = {};

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = m_context->physicalDevice;
        allocatorInfo.device = m_context->device;
        allocatorInfo.instance = m_context->instance;
        vkcheck(vmaCreateAllocator(&allocatorInfo, &m_context->memoryContext.allocator));
    }

    void Device::InitQueue()
    {
        m_queue = _new CommandQueue(this, Queue_Graphics | Queue_Compute | Queue_Transfer);
    }

    void Device::InitSwapchain(uint32_t width, uint32_t height)
    {
        check(width && height);
        if (m_swapchain.width == width && m_swapchain.height == height)
            return;
        DestroySwapchain();

        // Query surface properties
        VkSurfaceCapabilitiesKHR capabilities;
        vkcheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context->physicalDevice, m_context->surface, &capabilities));
        uint32_t formatCount = 0;
        vkcheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_context->physicalDevice, m_context->surface, &formatCount, nullptr));
        Mist::tFixedHeapArray<VkSurfaceFormatKHR> formats(formatCount);
        formats.Resize(formatCount);
        vkcheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_context->physicalDevice, m_context->surface, &formatCount, formats.GetData()));
        uint32_t presentModesCount = 0;
        vkcheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_context->physicalDevice, m_context->surface, &presentModesCount, nullptr));
        Mist::tFixedHeapArray<VkPresentModeKHR> presentModes(presentModesCount);
        presentModes.Resize(presentModesCount);
        vkcheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_context->physicalDevice, m_context->surface, &presentModesCount, presentModes.GetData()));

        // Find best surface format
        const ColorSpace desiredColorSpace = ColorSpace_SRGB;
        const Format desiredFormat = Format_R8G8B8A8_UNorm;
        uint32_t formatSelectedIndex = UINT32_MAX;
        for (uint32_t i = 0; i < formats.GetSize(); ++i)
        {
            if (formats[i].format == utils::ConvertFormat(desiredFormat))
            {
                check(utils::ConvertColorSpace(desiredColorSpace) == formats[i].colorSpace);
                formatSelectedIndex = i;
                break;
            }
        }
        check(formatSelectedIndex != UINT32_MAX);
        VkSurfaceFormatKHR surfaceFormat = formats[formatSelectedIndex];
        m_swapchain.format = desiredFormat;
        m_swapchain.colorSpace = desiredColorSpace;

        // Find best present mode
        uint32_t presentModeIndex = UINT32_MAX;
        const PresentMode desiredPresentMode = PresentMode_Immediate;
        for (uint32_t i = 0; i < presentModes.GetSize(); ++i)
        {
            if (presentModes[i] == utils::ConvertPresentMode(desiredPresentMode))
            {
                presentModeIndex = i;
                break;
            }
        }
        check(presentModeIndex != UINT32_MAX);
        VkPresentModeKHR presentMode;
        presentMode = presentModes[presentModeIndex];

        // Min images to present
        uint32_t imageCount = capabilities.minImageCount;
        logfinfo("Swapchain image count: %d (min:%2d; max:%2d)\n", imageCount,
            capabilities.minImageCount, capabilities.maxImageCount);

        // Check extent
        logfinfo("Swapchain current extent capabilities: (%4d x %4d) (%4d x %4d) (%4d x %4d)\n",
            capabilities.minImageExtent.width, capabilities.minImageExtent.height,
            capabilities.currentExtent.width, capabilities.currentExtent.height,
            capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            m_swapchain.width = capabilities.currentExtent.width;
            m_swapchain.height = capabilities.currentExtent.height;
        }
        else
        {
            m_swapchain.width = Mist::math::Clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            m_swapchain.height = Mist::math::Clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        VkExtent2D extent{m_swapchain.width, m_swapchain.height};

        // Image layers
        uint32_t imageLayers = __min(capabilities.maxImageArrayLayers, 1);

        // Image usage
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        check((imageUsage & capabilities.supportedUsageFlags) == imageUsage);

        // Pretransform
        VkSurfaceTransformFlagBitsKHR pretransform = capabilities.currentTransform;

        // Family queue indices
        uint32_t graphicsQueueIndex = FindFamilyQueueIndex(this, Queue_Graphics);
        check(graphicsQueueIndex != UINT32_MAX);
        VkBool32 graphicsSupportsPresent = VK_FALSE;
        vkcheck(vkGetPhysicalDeviceSurfaceSupportKHR(m_context->physicalDevice, graphicsQueueIndex, m_context->surface, &graphicsSupportsPresent));
        check(graphicsSupportsPresent);
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Create swapchain
        VkSwapchainCreateInfoKHR createInfo{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .pNext = nullptr };
        createInfo.surface = m_context->surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = imageLayers;
        createInfo.imageUsage = imageUsage;
        createInfo.imageSharingMode = sharingMode;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.preTransform = pretransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = true;
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        createInfo.flags = 0;
        vkcheck(vkCreateSwapchainKHR(m_context->device, &createInfo, m_context->allocationCallbacks, &m_swapchain.swapchain));

        uint32_t swapchainImageCount = 0;
        vkcheck(vkGetSwapchainImagesKHR(m_context->device, m_swapchain.swapchain, &swapchainImageCount, nullptr));
        check(swapchainImageCount != 0);
        VkImage* swapchainImages = _new VkImage[swapchainImageCount];
        vkcheck(vkGetSwapchainImagesKHR(m_context->device, m_swapchain.swapchain, &swapchainImageCount, swapchainImages));

        m_swapchain.images.resize(swapchainImageCount);
        Mist::tStaticArray<TextureBarrier, 8> barriers;
        for (uint32_t i = 0; i < swapchainImageCount; ++i)
        {
            TextureDescription desc;
            desc.extent = { m_swapchain.width, m_swapchain.height, 1 };
            desc.format = m_swapchain.format;
            desc.layers = imageLayers;
            desc.mipLevels = 1;
            desc.memoryUsage = MemoryUsage_Gpu;
            TextureHandle texture = CreateTextureFromNative(desc, swapchainImages[i]);
            m_swapchain.images[i] = texture;

            TextureBarrier& barrier = barriers.Push();
            barrier.texture = texture;
            barrier.newLayout = ImageLayout_PresentSrc;
        }
        check(!barriers.IsEmpty());
        CommandListHandle cmd = CreateCommandList();
        cmd->BeginRecording();
        cmd->SetTextureState(barriers.GetData(), barriers.GetSize());
        cmd->EndRecording();
        uint64_t id = cmd->ExecuteCommandList();
        check(WaitForSubmissionId(id));
        cmd = nullptr;
        delete[] swapchainImages;
    }

    void Device::DestroyMemoryContext()
    {
        check(m_context);
        check(m_context->memoryContext.bufferStats.allocationCounts == 0 && m_context->memoryContext.bufferStats.currentAllocated == 0);
        check(m_context->memoryContext.imageStats.allocationCounts == 0 && m_context->memoryContext.imageStats.currentAllocated == 0);

        vmaDestroyAllocator(m_context->memoryContext.allocator);
        m_context->memoryContext.allocator = VK_NULL_HANDLE;
    }

    void Device::DestroyContext()
    {
        check(m_context);
        vkDestroyDevice(m_context->device, m_context->allocationCallbacks);
        vkDestroySurfaceKHR(m_context->instance, m_context->surface, nullptr);
        if (m_context->debugMessenger)
        {
            PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_context->instance, "vkDestroyDebugUtilsMessengerEXT");
            check(pfnDestroyMessenger);
            pfnDestroyMessenger(m_context->instance, m_context->debugMessenger, m_context->allocationCallbacks);
        }
        vkDestroyInstance(m_context->instance, m_context->allocationCallbacks);
        delete m_context;
        m_context = nullptr;
    }

    void Device::DestroyQueue()
    {
        delete m_queue;
        m_queue = nullptr;
    }

    void Device::DestroySwapchain()
    {
        m_swapchain.images.clear();
        m_swapchain.images.shrink_to_fit();
        if (m_swapchain.swapchain)
            vkDestroySwapchainKHR(m_context->device, m_swapchain.swapchain, m_context->allocationCallbacks);
        m_swapchain.swapchain = VK_NULL_HANDLE;
    }
    
    void CreatePipelineLayout(Device* device, const BindingLayoutArray& bindingLayouts, VkPipelineLayout& pipelineLayout)
    {
        check(device);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pNext = nullptr;

        Mist::tStaticArray<VkDescriptorSetLayout, BindingLayout::MaxLayouts> layouts;
        layouts.Resize(bindingLayouts.GetSize());
        ZeroMem(layouts.GetData(), layouts.GetSize() * sizeof(VkDescriptorSetLayout));
        for (uint32_t i = 0; i < bindingLayouts.GetSize(); ++i)
        {
            check(bindingLayouts[i]->m_layout != VK_NULL_HANDLE);
            layouts[i] = bindingLayouts[i]->m_layout;
        }

        pipelineLayoutInfo.flags = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.setLayoutCount = layouts.GetSize();
        pipelineLayoutInfo.pSetLayouts = layouts.GetData();

        vkcheck(vkCreatePipelineLayout(device->GetContext().device, &pipelineLayoutInfo, device->GetContext().allocationCallbacks, &pipelineLayout));
    }

    uint32_t FindFamilyQueueIndex(Device* device, QueueType type)
    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device->GetContext().physicalDevice, &count, nullptr);
        VkQueueFamilyProperties* properties = _new VkQueueFamilyProperties[count];
        vkGetPhysicalDeviceQueueFamilyProperties(device->GetContext().physicalDevice, &count, properties);

        uint32_t familyIndex = UINT32_MAX;
        VkQueueFlags flags = utils::ConvertQueueFlags(type);
        for (uint32_t i = 0; i < count; ++i)
        {
            if ((properties[i].queueFlags & flags) == flags)
            {
                familyIndex = i;
                break;
            }
        }
        delete[] properties;
        return familyIndex;
    }


    TextureSubresourceRange TextureSubresourceRange::Resolve(const TextureDescription& description, bool singleMipLevel) const
    {
        TextureSubresourceRange range;

        // Mip levels
        range.baseMipLevel = baseMipLevel;
        if (singleMipLevel)
        {
            range.countMipLevels = 1;
        }
        else
        {
            uint32_t numMipLevels = __min(baseMipLevel + countMipLevels, description.mipLevels);
            range.countMipLevels = __max(0, numMipLevels - baseMipLevel);
        }

        // Layers
        switch (description.dimension)
        {
        case ImageDimension_Cube:
        case ImageDimension_1DArray:
        case ImageDimension_2DArray:
        case ImageDimension_CubeArray:
        {
            range.baseLayer = baseLayer;
            uint32_t numLayers = __min(baseLayer + countLayers, description.layers);
            range.countLayers = __max(0, numLayers - baseLayer);
        } break;
        default:
            range.baseLayer = 0;
            range.countLayers = 1;
            break;
        }

        return range;
    }

    BufferRange BufferRange::Resolve(const BufferDescription& description) const
    {
        BufferRange range;
        range.offset = __min(offset, description.size);
        range.size = size != 0 ? __min(size, description.size - range.offset) : description.size - range.offset;
        return range;
    }

    BindingSetItem BindingSetItem::CreateTextureSRVItem(uint32_t slot, TextureHandle texture, SamplerHandle sampler, ShaderType shaderStages, TextureSubresourceRange subresource, ImageDimension dimension)
    {
        BindingSetItem item;
        item.textures.Push(texture);
        item.samplers.Push(sampler);
        item.binding = slot;
        item.textureSubresources.Push(subresource);
        item.dimension = dimension;
        item.type = ResourceType_TextureSRV;
        item.shaderStages = shaderStages;
        return item;
    }

    BindingSetItem BindingSetItem::CreateTextureSRVItem(uint32_t slot, TextureHandle* textures, SamplerHandle* samplers, ShaderType shaderStages, TextureSubresourceRange* subresources, uint32_t count, ImageDimension dimension)
    {
        BindingSetItem item;
        for (uint32_t i = 0; i < count; ++i)
        {
            item.textures.Push(textures[i]);
            item.samplers.Push(samplers[i]);
            item.textureSubresources.Push(subresources[i]);
        }
        item.binding = slot;
        item.dimension = dimension;
        item.type = ResourceType_TextureSRV;
        item.shaderStages = shaderStages;
        return item;
    }

    BindingSetItem BindingSetItem::CreateTextureUAVItem(uint32_t slot, TextureHandle texture, ShaderType shaderStages, TextureSubresourceRange subresource, ImageDimension dimension)
    {
        BindingSetItem item;
		item.textures.Push(texture);
        item.binding = slot;
        item.textureSubresources.Push(subresource);
        item.dimension = dimension;
        item.type = ResourceType_TextureUAV;
        item.shaderStages = shaderStages;
        return item;
    }

	BindingSetItem BindingSetItem::CreateTextureUAVItem(uint32_t slot, TextureHandle* textures, ShaderType shaderStages, TextureSubresourceRange* subresources, uint32_t count, ImageDimension dimension /*= ImageDimension_Undefined*/)
	{
		BindingSetItem item;
		for (uint32_t i = 0; i < count; ++i)
		{
			item.textures.Push(textures[i]);
			item.textureSubresources.Push(subresources[i]);
		}
		item.binding = slot;
		item.dimension = dimension;
		item.type = ResourceType_TextureUAV;
		item.shaderStages = shaderStages;
		return item;
	}

	BindingSetItem BindingSetItem::CreateConstantBufferItem(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange)
    {
        BindingSetItem item;
        item.buffer = buffer;
        item.binding = slot;
        item.bufferRange = bufferRange;
        item.type = ResourceType_ConstantBuffer;
        item.dimension = ImageDimension_Undefined;
        item.shaderStages = shaderStages;
        return item;
    }

    BindingSetItem BindingSetItem::CreateVolatileConstantBufferItem(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange)
    {
        BindingSetItem item;
        item.buffer = buffer;
        item.binding = slot;
        item.bufferRange = bufferRange;
        item.type = ResourceType_VolatileConstantBuffer;
        item.dimension = ImageDimension_Undefined;
        item.shaderStages = shaderStages;
        return item;
    }

    BindingSetItem BindingSetItem::CreateBufferUAVItem(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange)
    {
        BindingSetItem item;
        item.buffer = buffer;
        item.binding = slot;
        item.bufferRange = bufferRange;
        item.type = ResourceType_BufferUAV;
        item.dimension = ImageDimension_Undefined;
        item.shaderStages = shaderStages;
        return item;
    }

    BindingSet::~BindingSet()
    {
        check(m_device);
        m_device->DestroyBindingSet(this);
    }

    namespace utils
    {
        BufferHandle CreateBufferAndUpload(Device* device, const void* buffer, uint64_t size, BufferUsage usage, MemoryUsage memoryUsage, UploadContext* uploadContext, const char* debugName)
        {
            BufferDescription desc;
            desc.size = size;
            desc.bufferUsage = usage;
            desc.memoryUsage = memoryUsage;
            if (debugName && *debugName)
                desc.debugName = debugName;
            BufferHandle h = device->CreateBuffer(desc);

            if (uploadContext)
            {
                uploadContext->WriteBuffer(h, buffer, size);
            }
            else
            {
                UploadContext context(device);
                context.WriteBuffer(h, buffer, size);
                context.Submit();
            }
            return h;
        }

        BufferHandle CreateVertexBuffer(Device* device, const void* buffer, uint64_t bufferSize, UploadContext* uploadContext, const char* debugName)
        {
            return CreateBufferAndUpload(device, buffer, bufferSize, BufferUsage_VertexBuffer, MemoryUsage_Gpu, uploadContext, debugName);
        }

        BufferHandle CreateIndexBuffer(Device* device, const void* buffer, uint64_t bufferSize, UploadContext* uploadContext, const char* debugName)
        {
            return CreateBufferAndUpload(device, buffer, bufferSize, BufferUsage_IndexBuffer, MemoryUsage_Gpu, uploadContext, debugName);
        }

        BufferHandle CreateUniformBuffer(Device* device, uint64_t size, const char* debugName)
        {
            BufferDescription desc;
            desc.size = device->AlignUniformSize(size);
            desc.bufferUsage = BufferUsage_UniformBuffer;
            desc.memoryUsage = MemoryUsage_CpuToGpu;
            if (debugName && *debugName)
                desc.debugName = debugName;
            return device->CreateBuffer(desc);
        }

        BufferHandle CreateDynamicVertexBuffer(Device* device, uint64_t bufferSize, const char* debugName)
        {
            BufferDescription desc;
            desc.size = bufferSize;
            desc.memoryUsage = MemoryUsage_CpuToGpu;
            desc.bufferUsage = BufferUsage_VertexBuffer;
            if (debugName && *debugName)
                desc.debugName = debugName;
            return device->CreateBuffer(desc);
        }

        UploadContext::UploadContext()
            : m_device(nullptr), m_cmd(nullptr) { }

        UploadContext::UploadContext(Device* device)
            : m_device(nullptr), m_cmd(nullptr)
        {
            Init(device);
        }

        UploadContext::~UploadContext()
        {
            Destroy();
        }

        void UploadContext::Init(Device* device)
        {
            check(!m_device && !m_cmd && device);
            m_device = device;
            m_cmd = m_device->CreateCommandList();
            BeginRecording();
        }

        void UploadContext::Destroy(bool waitForSubmission)
        {
            if (!m_cmd || !m_device)
                return;
            Submit(waitForSubmission);
            m_cmd = nullptr;
            m_device = nullptr;
        }

        void UploadContext::SetTextureLayout(TextureHandle texture, ImageLayout layout, uint32_t layer, uint32_t mipLevel)
        {
            TextureBarrier barrier;
            barrier.texture = texture;
            barrier.newLayout = layout;
            barrier.subresources = { mipLevel, 1, layer, 1 };
            m_cmd->RequireTextureState(barrier);
        }

        void UploadContext::WriteTexture(TextureHandle texture, uint32_t mipLevel, uint32_t layer, const void* data, size_t dataSize)
        {
            m_cmd->WriteTexture(texture, mipLevel, layer, data, dataSize);
        }

        void UploadContext::WriteBuffer(BufferHandle buffer, const void* data, uint64_t dataSize, uint64_t srcOffset, uint64_t dstOffset)
        {
            m_cmd->WriteBuffer(buffer, data, dataSize, srcOffset, dstOffset);
        }

        void UploadContext::Blit(const BlitDescription& desc)
        {
            m_cmd->BlitTexture(desc);
        }

        uint64_t UploadContext::Submit(bool waitForSubmission)
        {
            if (!m_cmd->IsRecording())
                return 0;
            m_cmd->EndRecording();
            uint64_t submission = m_cmd->ExecuteCommandList();
            if (waitForSubmission)
                m_device->WaitForSubmissionId(submission);
            return submission;
        }

		void UploadContext::BeginRecording()
		{
			if (!m_cmd->IsRecording())
				m_cmd->BeginRecording();
		}

		ShaderHandle BuildShader(Device* device, const char* filepath, ShaderType type)
        {
            shader_compiler::CompiledBinary bin = shader_compiler::Compile(filepath, type);
            check(bin.IsCompilationSucceed());
            ShaderDescription desc;
            desc.debugName = filepath;
            desc.name = filepath;
            desc.type = type;
            desc.entryPoint = "main";
            ShaderHandle h = device->CreateShader(desc, bin.binary, bin.binaryCount);
            return h;
        }

        ShaderHandle BuildVertexShader(Device* device, const char* filepath)
        {
            return BuildShader(device, filepath, ShaderType_Vertex);
        }

        ShaderHandle BuildFragmentShader(Device* device, const char* filepath)
        {
            return BuildShader(device, filepath, ShaderType_Fragment);
        }

    }

    TextureSubresourceLayer TextureSubresourceLayer::Resolve(const TextureDescription& desc) const
    {
        check(desc.layers > 0);
        TextureSubresourceLayer subresource;
        subresource.mipLevel = __min(desc.mipLevels, mipLevel);
        subresource.layer = __min(desc.layers-1, layer);
        subresource.layerCount = __min(desc.layers - subresource.layer, layerCount);
        return subresource;
    }
}
