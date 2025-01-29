#pragma once

#include "Core/Types.h"
#include "Render/RenderContext.h"

namespace Mist
{
    class ShaderProgram;
    class RenderTarget;

    enum EQueueTypeBits
    {
        QUEUE_NONE = 0,
        QUEUE_GRAPHICS = 1,
        QUEUE_COMPUTE = 2,
        QUEUE_TRANSFER = 4
    };
    typedef uint8_t EQueueType;

    struct CommandBuffer
    {
        VkCommandBuffer CmdBuffer;
        VkCommandPool Pool;
        EQueueType Type;
        uint32_t SubmissionId;

        void Begin();
        void End();
    };

    class CommandQueue
    {
        static constexpr uint32_t MaxSemaphores = 8;
    public:
        CommandQueue(const RenderContext* context, EQueueType type);
        ~CommandQueue();

        CommandBuffer* CreateCommandBuffer();

        uint64_t Submit(CommandBuffer* const* cmds, uint32_t count);

        void AddWaitSemaphore(VkSemaphore semaphore, uint64_t value);
        void AddSignalSemaphore(VkSemaphore semaphore, uint64_t value);
        void AddWaitQueue(const CommandQueue* queue, uint64_t value);

        uint64_t QueryTrackingId();
        void ProcessInFlightCommands();

        bool PollCommandSubmission(uint64_t submissionId);
        bool WaitForCommandSubmission(uint64_t submissionId, uint64_t timeout = 1);

        inline uint64_t GetLastSubmissionId() const { return m_submissionId; }
        inline uint64_t GetLastFinishedId() const { return m_lastFinishedId; }
        inline EQueueType GetQueueType() const { return m_type; }

    private:
        // Context reference
        const RenderContext* m_context;
        // Queue related
        VkQueue m_queue;
        uint32_t m_queueFamilyIndex;
        EQueueType m_type;

        uint64_t m_submissionId;
        uint64_t m_lastFinishedId;
        // Timeline type semaphore.
        VkSemaphore m_trackingSemaphore;

        tFixedHeapArray<CommandBuffer*> m_commandPool;
        tFixedHeapArray<CommandBuffer*> m_submittedCmds;
        tFixedHeapArray<CommandBuffer*> m_createdBuffers;

        tStaticArray<VkSemaphore, MaxSemaphores> m_waitSemaphores;
        tStaticArray<uint64_t, MaxSemaphores> m_waitSemaphoreValues;
        tStaticArray<VkSemaphore, MaxSemaphores> m_signalSemaphores;
        tStaticArray<uint64_t, MaxSemaphores> m_signalSemaphoreValues;
    };

    struct GraphicsState
    {
        ShaderProgram* Program;
        RenderTarget* Rt;

        inline constexpr bool operator==(const GraphicsState& other) const { return Program == other.Program && Rt == other.Rt; }
    };

    struct ComputeState
    {
        ShaderProgram* Program;
    };

    class CommandList
    {
    public:

        CommandList(const RenderContext* context);

        void Begin();
        void End();

        void BeginMarker(const char* name);
        void EndMarker();

        void SetGraphicsState(const GraphicsState& state);

        void BindVertexBuffer(VertexBuffer vbo);
        void BindIndexBuffer(IndexBuffer ibo);
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset);

        // TODO: set depth stencil state
        // TODO: set blending state

        inline CommandBuffer* GetCurrentCommandBuffer() const { return m_currentCmd; }

    private:
        void EndRenderPass();
        void ClearState();

    private:
        const RenderContext* m_context;
        GraphicsState m_graphicsState;
        ComputeState m_computeState;
        CommandBuffer* m_currentCmd;
        EQueueType m_type;
    };
}
