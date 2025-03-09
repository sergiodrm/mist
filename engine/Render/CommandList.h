#pragma once

#include "Core/Types.h"
#include "Render/RenderContext.h"

namespace Mist
{
    class ShaderProgram;
    class RenderTarget;
    class cTexture;

    enum EQueueTypeBits
    {
        QUEUE_NONE = 0,
        QUEUE_GRAPHICS = 1,
        QUEUE_COMPUTE = 2,
        QUEUE_TRANSFER = 4
    };
    typedef uint8_t EQueueType;

    class FamilyQueueIndexFinder
    {
    public:
        static constexpr uint32_t InvalidFamilyQueueIndex = UINT32_MAX;

        FamilyQueueIndexFinder(const RenderContext& context);
        ~FamilyQueueIndexFinder();

        // Returns InvalidFamilyQueueIndex if failed.
        uint32_t FindFamilyQueueIndex(EQueueType type);
        void DumpInfo() const;

        // Returns InvalidFamilyQueueIndex if failed.
        static uint32_t FindFamilyQueueIndex(const RenderContext& context, EQueueType type);

    private:
        const RenderContext* m_context = nullptr;
        tFixedHeapArray<VkQueueFamilyProperties> m_properties;
    };

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

    struct SubresourceRange
    {
        static constexpr uint32_t RemainingRange = UINT32_MAX;
        uint32_t BaseMipLevel;
        uint32_t LevelCount = RemainingRange;
        uint32_t BaseArrayLayer;
        uint32_t LayerCount = RemainingRange;

        inline void SetAllLevels()
        {
            BaseMipLevel = 0;
            LevelCount = RemainingRange;
        }
        inline void SetAllLayers()
        {
            BaseArrayLayer = 0;
            LayerCount = RemainingRange;
        }
        inline void SetAll()
        {
            SetAllLevels();
            SetAllLayers();
        }
    };

    struct TextureBarrier
    {
        const cTexture* Texture;
        EImageLayout OldLayout;
        EImageLayout NewLayout;
        SubresourceRange Subresource;

        inline void SetAllSubresources()
        {
            Subresource.SetAll();
        }
    };

    struct GraphicsState
    {
        ShaderProgram* Program;
        RenderTarget* Rt;
        VertexBuffer Vbo;
        IndexBuffer Ibo;

        inline constexpr bool operator==(const GraphicsState& other) const 
        { 
            return Program == other.Program 
                && Rt == other.Rt
                && Vbo == other.Vbo
                && Ibo == other.Ibo; 
        }
    };

    struct ComputeState
    {
        ShaderProgram* Program;

        inline constexpr bool operator==(const ComputeState& other) const { return Program == other.Program; }
    };

    class CommandList
    {
        static constexpr uint32_t MaxBarriers = 16;
    public:

        enum ECommandListState : uint8_t
        {
            STATE_NONE,
            STATE_GRAPHICS,
            STATE_COMPUTE
        };

        CommandList(const RenderContext* context);
        ~CommandList();

        static uint64_t ExecuteCommandLists(CommandList* const* lists, uint32_t count);

        void Begin();
        void End();

        void BeginMarker(const char* name);
        void EndMarker();
        // TODO: gpu query for time markers.

        // Graphics related
        inline const GraphicsState& GetGraphicsState() const { return m_graphicsState; }
        void SetGraphicsState(const GraphicsState& state);
        void ClearColor(float r = 0.f, float g = 0.f, float b = 0.f, float a = 0.f);
        void ClearDepthStencil(float depth = 1.f, uint32_t stencil = 0);
        void BindVertexBuffer(VertexBuffer vbo);
        void BindIndexBuffer(IndexBuffer ibo);
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset);

        // Viewport operations
        void SetViewport(float x, float y, float width, float height, float minDepth = 0.f, float maxDepth = 1.f);
        void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

        // TODO: set depth stencil state
        // TODO: set blending state

        // Compute related
        void SetComputeState(const ComputeState& state);
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        // Transfer related
        void CopyBuffer(AllocatedBuffer src, AllocatedBuffer dst, uint32_t size, uint32_t srcOffset = 0, uint32_t dstOffset = 0);

        // TODO: resources state required to be set before draw or dispatch
        // TODO: pipeline barriers?
        void SetTextureState(const TextureBarrier* barriers, uint32_t count);
        void SetTextureState(const TextureBarrier& barrier);

        // TODO: binding resources
        void BindDescriptorSets(const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet = 0, const uint32_t* dynamicOffsetArray = nullptr, uint32_t dynamicOffsetCount = 0);
        void BindProgramDescriptorSets();

        inline CommandBuffer* GetCurrentCommandBuffer() const { return m_currentCmd; }
        inline ShaderProgram* GetCurrentProgram() const { return m_state != STATE_NONE ? (m_state == STATE_GRAPHICS ? m_graphicsState.Program : m_computeState.Program) : nullptr; }
        inline bool IsRecording() const { return m_currentCmd != nullptr; }

        void ClearState();
        inline bool CheckCommandQueueType(EQueueType type) const { return m_currentCmd && m_currentCmd->Type & type; }
        inline bool CheckGraphicsCommandType() const { return CheckCommandQueueType(QUEUE_GRAPHICS); }
        inline bool CheckComputeCommandType() const { return CheckCommandQueueType(QUEUE_COMPUTE); }
        inline bool CheckTransferCommandType() const { return CheckCommandQueueType(QUEUE_TRANSFER); }
    private:
        void EndRenderPass();

        void Executed();

    private:
        const RenderContext* m_context;
        GraphicsState m_graphicsState;
        ComputeState m_computeState;
        CommandBuffer* m_currentCmd;
        ECommandListState m_state;
    };
}
