#pragma once

#include "RenderAPI/Device.h"
#include "RenderAPI/Utils.h"
#include "RenderAPI/ShaderCompiler.h"
#include <glm/glm.hpp>
#include "Utils/FileSystem.h"

namespace rendersystem
{
    template <typename DataType, typename IndexType = uint32_t>
    struct HeapArray
    {
        DataType* data = nullptr;
        IndexType count = 0;

        void Reserve(IndexType _count)
        {
            check(_count > 0);
            if (!data)
            {
                check(count == 0);
                data = _new DataType[_count];
                count = _count;
            }
            else
            {
                check(count > 0);
                if (count < _count)
                {
                    DataType* p = (DataType*)_realloc(data, _count);
                    check(p);
                    data = p;
                    count = _count;
                }
            }
        }

        void Release()
        {
            if (data)
            {
                delete[] data;
                data = nullptr;
                count = 0;
            }
        }
    };

    enum ShaderProgramType
    {
        ShaderProgram_Graphics,
        ShaderProgram_Compute,
    };

    class IWindow
    {
    public:
        virtual void* GetWindowHandle() const = 0;
        virtual void* GetWindowNative() const = 0;
    };

    class GpuFrameProfiler
    {
        enum
        {
            State_Idle,
            State_Recording
        };
        static constexpr size_t MaxSize = 128;
    public:
		struct QueryEntry
		{
			enum { InvalidQueryId = UINT32_MAX };
			uint32_t id = InvalidQueryId;
			double value = 0.0;
			char tag[32];

			~QueryEntry()
			{
				id = InvalidQueryId;
				value = 0.0;
				*tag = 0;
			}
		};

        typedef Mist::tStackTree<QueryEntry, MaxSize> QueryTree;

        GpuFrameProfiler(render::Device* device);

        // Insert marks inside command list.
        void BeginZone(const render::CommandListHandle& cmd, const char* tag);
        void EndZone(const render::CommandListHandle& cmd);
        void Reset(const render::CommandListHandle& cmd);
        bool CanRecord() const;

        // Collect data from gpu queries once CommandList has been submitted and finished.
        void Resolve();
        void ImGuiDraw();

        const render::QueryPoolHandle& GetPool() const { return m_pool; }

        double GetTimestamp(const char* tag);

        const QueryTree& GetData() const { return m_tree; }
        static void ImGuiDrawQueryTree(const char* title, const QueryTree& tree);

    private:
        static void ImGuiDrawTreeItem(const QueryTree& tree, uint32_t itemIndex);
        void ImGuiDrawTreeItem(uint32_t itemIndex);

    private:
        render::Device* m_device;
        render::QueryPoolHandle m_pool;
        uint32_t m_queryId;
        uint32_t m_state;

       
        QueryTree m_tree;
    };

    class BindingLayoutCache
    {
    public:
        BindingLayoutCache(render::Device* device) : m_device(device) {}
        ~BindingLayoutCache() { m_cache.clear(); }

        render::BindingLayoutHandle GetCachedLayout(const render::BindingLayoutDescription& desc);
    private:
        render::Device* m_device;
        Mist::tMap<render::BindingLayoutDescription, render::BindingLayoutHandle> m_cache;
    };

    class BindingCache
    {
    public:
        BindingCache(render::Device* device) : m_device(device), m_layoutCache(device) {}
        ~BindingCache() { m_cache.clear(); }
        render::BindingSetHandle GetCachedBindingSet(const render::BindingSetDescription& desc);
        uint32_t GetCacheSize() const { return (uint32_t)m_cache.size(); }
        float GetLoadFactor() const { return m_cache.load_factor(); }
        float GetMaxLoadFactor() const { return m_cache.max_load_factor(); }
    private:
        render::Device* m_device;
        Mist::tMap<render::BindingSetDescription, render::BindingSetHandle> m_cache;
        BindingLayoutCache m_layoutCache;
    };

    class ShaderMemoryContext
    {
    public:
        struct PropertyMemory
        {
            render::BufferHandle buffer;
            uint64_t offset = 0;
            uint64_t size = 0;
        };

        ShaderMemoryContext(render::Device* device);
        ~ShaderMemoryContext();
        ShaderMemoryContext(const ShaderMemoryContext& other);
        ShaderMemoryContext(ShaderMemoryContext&& rvl);
        ShaderMemoryContext& operator=(const ShaderMemoryContext& other);
        ShaderMemoryContext& operator=(ShaderMemoryContext&& rvl);

        void ReserveProperty(const char* id, uint64_t size);
        void WriteProperty(const char* id, const void* data, uint64_t size);
        const PropertyMemory* GetProperty(const char* id) const;

        void BeginFrame();
        void FlushMemory();
        static constexpr uint64_t MinTempBufferSize() { return (1 << 14); }
    private:
        void ResizeTempBuffer(uint64_t size);
        void Write(const void* data, uint64_t size, uint64_t srcOffset = 0, uint64_t dstOffset = 0);
        uint32_t GetOrCreateBuffer(uint64_t size);
        void Invalidate();
    public:
        render::Device* m_device;
        uint8_t* m_tempBuffer = nullptr;
        uint64_t m_pointer = 0;
        uint64_t m_size = 0;
        uint64_t m_submissionId = UINT64_MAX;

        Mist::tDynArray<render::BufferHandle> m_buffers;
        Mist::tDynArray<uint32_t> m_freeBuffers;
        Mist::tDynArray<uint32_t> m_usedBuffers;
        Mist::tMap<Mist::String, PropertyMemory> m_properties;
    };

    class ShaderMemoryPool
    {
    public:
        ShaderMemoryPool(render::Device* device);

        uint32_t CreateContext();
        ShaderMemoryContext* GetContext(uint32_t context);
        void Submit(uint64_t submissionId, uint32_t* contexts, uint32_t count);
        void ProcessInFlight();

    private:
        render::Device* m_device;
        Mist::tDynArray<ShaderMemoryContext> m_contexts;
        Mist::tDynArray<uint32_t> m_freeContexts;
        Mist::tDynArray<uint32_t> m_usedContexts;
    };

    class TextureCache
    {
    public:
        TextureCache(render::Device* device);
        ~TextureCache();
        render::TextureHandle GetTexture(const char* name) const;
        render::TextureHandle GetOrCreateTexture(const render::TextureDescription& desc);
        // iterate over all textures and remove it if there is no external refs and reorganize free indices.
        void Purge();
    private:
        render::Device* m_device;
        HeapArray<render::TextureHandle> m_textures;
        uint32_t m_pushIndex;
        Mist::tMap<Mist::String, uint32_t> m_map;
    };

    class SamplerCache
    {
    public:
        SamplerCache(render::Device* device);
        ~SamplerCache();
        render::SamplerHandle GetSampler(const render::SamplerDescription& desc);
        uint32_t GetCacheSize() const { return (uint32_t)m_samplers.size(); }
        float GetLoadFactor() const { return m_samplers.load_factor(); }
        float GetMaxLoadFactor() const { return m_samplers.max_load_factor(); }
    public:
        render::Device* m_device;
        Mist::tMap<render::SamplerDescription, render::SamplerHandle> m_samplers;
    };

    struct ShaderFileDescription
    {
        Mist::cAssetPath filePath;
        render::shader_compiler::CompilationOptions options;

        inline bool operator ==(const ShaderFileDescription& other) const
        {
            return filePath == other.filePath
                && options == other.options;
        }

        inline bool operator!=(const ShaderFileDescription& other) const { return !(*this == other); }
    };

    struct ShaderDynamicBufferDescription
    {
        Mist::String name;
        uint32_t count;

        inline bool operator ==(const ShaderDynamicBufferDescription& other) const
        {
            return count == other.count
                && name == other.name;
        }

        inline bool operator!=(const ShaderDynamicBufferDescription& other) const { return !(*this == other); }
    };

    struct ShaderBuildDescription
    {
        ShaderProgramType type = ShaderProgram_Graphics;
        ShaderFileDescription vsDesc;
        ShaderFileDescription fsDesc;
        ShaderFileDescription csDesc;

        Mist::tDynArray<ShaderDynamicBufferDescription> dynamicBuffers;

        inline bool operator ==(const ShaderBuildDescription& other) const
        {
            return type == other.type
                && vsDesc == other.vsDesc
                && fsDesc == other.fsDesc
                && csDesc == other.csDesc
                && render::utils::EqualArrays(dynamicBuffers.data(), (uint32_t)dynamicBuffers.size(),
                    other.dynamicBuffers.data(), (uint32_t)other.dynamicBuffers.size());
        }
        inline bool operator!=(const ShaderBuildDescription& other) const { return !(*this == other); }
    };

    class ShaderProgram
    {
        friend class RenderSystem;
        ShaderProgram(render::Device* device, const ShaderBuildDescription& description);
    public:
        ~ShaderProgram();
        void Reload();
        bool IsLoaded() const;
        void ReleaseResources();

        render::ShaderHandle GetVertexShader() const { return m_vs; }
        render::ShaderHandle GetFragmentShader() const { return m_fs; }
        render::ShaderHandle GetComputeShader() const { return m_cs; }

        const render::shader_compiler::ShaderPropertyDescription* GetPropertyDescription(const char* id, uint32_t* setIndexOut) const;

    private:
        bool ReloadGraphics();
        bool ReloadCompute();

        render::Device* m_device;
    public:
        render::ShaderHandle m_vs;
        render::ShaderHandle m_fs;
        render::ShaderHandle m_cs;
        render::VertexInputLayout m_inputLayout;
        render::shader_compiler::ShaderReflectionProperties* m_properties;
        ShaderBuildDescription* m_description;
    };

    class ShaderDb
    {
    public:
        ShaderDb()
        {
            m_programs.reserve(10);
        }

        ~ShaderDb()
        {
            check(m_programs.empty());
        }

        ShaderDb(const ShaderDb&) = delete;
        ShaderDb(ShaderDb&&) = delete;
        ShaderDb& operator=(const ShaderDb&) = delete;
        ShaderDb& operator=(ShaderDb&&) = delete;

        void AddProgram(ShaderProgram* program)
        {
            check(program);
            m_programs.push_back(program);
        }

        void RemoveProgram(ShaderProgram* program)
        {
            for (uint32_t i = (uint32_t)m_programs.size()-1; i < (uint32_t)m_programs.size(); --i)
            {
                if (program == m_programs[i])
                {
                    if (i != (uint32_t)m_programs.size() - 1)
                        m_programs[i] = m_programs.back();
                    m_programs.pop_back();
                    return;
                }
            }
            unreachable_code();
        }

        void ReloadAll()
        {
            for (uint32_t i = 0; i < (uint32_t)m_programs.size(); ++i)
            {
                m_programs[i]->Reload();
                check(m_programs[i]->IsLoaded());
            }
        }


        Mist::tDynArray<ShaderProgram*> m_programs;
    };

    class RenderSystem
    {
    public:
        static constexpr uint32_t MaxDescriptorSetSlots = 8;
        static constexpr uint32_t MaxBindingsPerSlot = 8;
        static constexpr uint32_t MaxTextureArrayCount = 8;

    private:
        struct GraphicsPipelineContext
        {
            render::GraphicsState graphicsState;
			render::GraphicsPipelineDescription pso;

            bool pendingClearColor;
            float clearColor[4];

            bool pendingClearDepthStencil;
            float clearDepth;
            uint32_t clearStencil;

            void Invalidate()
            {
                graphicsState = render::GraphicsState();
                pso = render::GraphicsPipelineDescription();
            }
        };

        struct ComputePipelineContext
        {
			render::ComputePipelineDescription pso;
            render::ComputeState computeState;

            void Invalidate()
            {
                pso = render::ComputePipelineDescription();
                computeState = render::ComputeState();
            }
        };

        struct ShaderContext
        {
			render::TextureHandle textureSlots[MaxDescriptorSetSlots][MaxBindingsPerSlot][MaxTextureArrayCount];
			render::SamplerHandle samplerSlots[MaxDescriptorSetSlots][MaxBindingsPerSlot][MaxTextureArrayCount];
            render::BufferHandle buffers[MaxDescriptorSetSlots][MaxBindingsPerSlot];
			ShaderProgram* program;
			uint32_t dirtyPropertiesFlags;

            void Invalidate()
            {
                dirtyPropertiesFlags = UINT32_MAX;
                program = nullptr;
                for (uint32_t i = 0; i < MaxDescriptorSetSlots; ++i)
                {
                    for (uint32_t j = 0; j < MaxBindingsPerSlot; ++j)
                    {
                        for (uint32_t k = 0; k < MaxTextureArrayCount; ++k)
                        {
                            textureSlots[i][j][k] = nullptr;
                            samplerSlots[i][j][k] = nullptr;
                        }
                        buffers[i][j] = nullptr;
                    }
                }
            }

            inline void MarkSetAsDirty(uint32_t setIndex)
            {
                dirtyPropertiesFlags |= (1 << setIndex);
            }

            inline bool IsSetDirty(uint32_t setIndex)
            {
                return dirtyPropertiesFlags & (1 << setIndex);
            }

            inline void ClearDirtyAll()
            {
                dirtyPropertiesFlags = 0xffffffff;
            }

            inline void ClearDirtySet(uint32_t setIndex)
            {
                dirtyPropertiesFlags &= ~(1 << setIndex);
            }
        };

        // Data struct for keeping tracking of resources used in each frame.
        // Once the submission is completed, call Clear() method.
        struct FrameResourceTrack
        {
            Mist::tDynArray<render::BufferHandle> buffers;

            FrameResourceTrack(uint32_t initialCapacity = 10)
                : buffers(initialCapacity) { }

            void Clear()
            {
                buffers.clear();
            }
        };
    public:

        void Init(IWindow* window);
        void Destroy();

        void BeginFrame();
        void EndFrame();

        void BeginMarker(const char* name, render::Color color = render::Color::White());
        void BeginMarkerFmt(const char* fmt, ...);
        void EndMarker();

        void BeginGpuProf(const char* name);
        void BeginGpuProfFmt(const char* fmt, ...);
        void EndGpuProf();

        inline render::RenderTargetHandle GetLDRTarget() const { return m_ldrRt; }
        inline render::TextureHandle GetLDRTexture() const { return m_ldrTexture; }

        inline const render::Extent2D& GetRenderResolution() const { return m_renderResolution; }
        inline const render::Extent2D& GetBackbufferResolution() const { return m_backbufferResolution; }

        render::Device* GetDevice() const { return m_device; }
        ShaderProgram* CreateShader(const ShaderBuildDescription& desc);
        void DestroyShader(ShaderProgram** shader);
        void ReloadAllShaders();

        inline uint64_t GetFrameIndex() const { return m_frame % m_device->GetSwapchain().images.size(); }
        render::BindingSetHandle GetBindingSet(const render::BindingSetDescription& desc);
        render::SamplerHandle GetSampler(const render::SamplerDescription& desc);
        render::SamplerHandle GetSampler(render::Filter minFilter, 
            render::Filter magFilter, 
            render::Filter mipmapMode, 
            render::SamplerAddressMode addressModeU,
            render::SamplerAddressMode addressModeV,
            render::SamplerAddressMode addressModeW);


        void ClearState();

        /**
         * Graphics commands
         */

        void SetDefaultGraphicsState();
        void SetDepthEnable(bool testing = true, bool writing = true);
        void SetDepthOp(render::CompareOp op = render::CompareOp_Less);
        void SetStencilEnable(bool testing = false);
        void SetStencilMask(uint8_t readMask = 0xff, uint8_t writeMask = 0xff, uint8_t refValue = 0);
        void SetStencilOpFront(render::StencilOp fail = render::StencilOp_Keep,
            render::StencilOp depthFail = render::StencilOp_Keep,
            render::StencilOp pass = render::StencilOp_Keep,
            render::CompareOp compareOp = render::CompareOp_Always);
        void SetStencilOpBack(render::StencilOp fail = render::StencilOp_Keep,
            render::StencilOp depthFail = render::StencilOp_Keep,
            render::StencilOp pass = render::StencilOp_Keep,
            render::CompareOp compareOp = render::CompareOp_Always);
        inline void SetStencilOpFrontAndBack(render::StencilOp fail = render::StencilOp_Keep,
            render::StencilOp depthFail = render::StencilOp_Keep,
            render::StencilOp pass = render::StencilOp_Keep,
            render::CompareOp compareOp = render::CompareOp_Always)
        {
			SetStencilOpFront(fail, depthFail, pass, compareOp);
			SetStencilOpBack(fail, depthFail, pass, compareOp);
        }

        void SetBlendEnable(bool enabled = false, uint32_t attachment = 0);
        void SetBlendFactor(render::BlendFactor srcBlend = render::BlendFactor_One,
            render::BlendFactor dstBlend = render::BlendFactor_Zero,
            render::BlendOp blendOp = render::BlendOp_Add, uint32_t attachment = 0);
        void SetBlendAlphaState(render::BlendFactor srcBlend = render::BlendFactor_One,
            render::BlendFactor dstBlend = render::BlendFactor_Zero,
            render::BlendOp blendOp = render::BlendOp_Add, uint32_t attachment = 0);
        void SetBlendWriteMask(render::ColorMask mask = render::ColorMask_All, uint32_t attachment = 0);

        void SetViewport(float x, float y, float width, float height, float minDepth = 0.f, float maxDepth = 1.f);
        void SetViewport(const render::Viewport& viewport);
        void SetScissor(float x0, float x1, float y0, float y1);
        void SetScissor(const render::Rect& scissor);

        void SetFillMode(render::RasterFillMode mode = render::RasterFillMode_Fill);
        void SetCullMode(render::RasterCullMode mode = render::RasterCullMode_Back);
        void SetPrimitive(render::PrimitiveType type = render::PrimitiveType_TriangleList);


		void SetRenderTarget(render::RenderTargetHandle rt);

		void SetVertexBuffer(render::BufferHandle vb);
		void SetIndexBuffer(render::BufferHandle ib);
		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
		void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, uint32_t firstVertex = 0, uint32_t firstInstance = 0);

        /**
         * Compute commands
         */
        void Dispatch(uint32_t workgroupSizeX, uint32_t workgroupSizeY, uint32_t workgroupSizeZ);

        /**
         * Shader properties methods
         */
        void SetShader(ShaderProgram* shader);
        void SetTextureSlot(const render::TextureHandle& texture, uint32_t set, uint32_t binding = 0, uint32_t textureIndex = 0);
        void SetTextureSlot(const char* id, const render::TextureHandle& texture);
        void SetTextureSlot(const render::TextureHandle* textures, uint32_t count, uint32_t set, uint32_t binding = 0);
        void SetTextureSlot(const char* id, const render::TextureHandle* textures, uint32_t count);
        void SetSampler(const render::SamplerHandle& sampler, uint32_t set, uint32_t binding = 0, uint32_t samplerIndex = 0);
        void SetSampler(const render::SamplerHandle* sampler, uint32_t count, uint32_t set, uint32_t binding = 0);
        void SetSampler(const char* id, const render::SamplerHandle& sample);
        void SetSampler(const char* id, const render::SamplerHandle* sample, uint32_t count);
        void SetSampler(const char* id, render::Filter minFilter, render::Filter magFilter,
            render::Filter mipmapMode,
            render::SamplerAddressMode addressModeU,
            render::SamplerAddressMode addressModeV,
            render::SamplerAddressMode addressModeW,
            uint32_t samplerIndex = 0);
        void SetSampler(render::Filter minFilter, render::Filter magFilter,
            render::Filter mipmapMode,
            render::SamplerAddressMode addressModeU,
            render::SamplerAddressMode addressModeV,
            render::SamplerAddressMode addressModeW,
            uint32_t set, uint32_t binding = 0, uint32_t samplerIndex = 0);
        void SetShaderProperty(const char* id, const void* param, uint64_t size);
        void SetTextureLayout(const render::TextureHandle& texture, render::ImageLayout layout, render::TextureSubresourceRange range = {0,1,0,1});
        void SetTextureAsResourceBinding(render::TextureHandle texture);
        void SetTextureAsRenderTargetAttachment(render::TextureHandle texture);

        void BindUAV(const char* id, const render::BufferHandle& buffer);
        void BindSRV(const char* id, const render::BufferHandle& buffer);

        void BindUAV(const char* id, const render::TextureHandle& texture);


        ShaderMemoryPool* GetMemoryPool() const { return m_memoryPool; }
        const render::CommandListHandle& GetCommandList() const { return m_cmd; }

        /**
         * Transfer functions
         */
        void CopyTextureToTexture(const render::TextureHandle& src, const render::TextureHandle& dst, const render::CopyTextureInfo* infoArray, uint32_t infoCount);

        // Utilities
        void DrawFullscreenQuad();
        void ClearColor(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);
        void ClearDepthStencil(float depth = 1.f, uint32_t stencil = 0);

        void DumpState();

        double GetGpuTimeUs() const { return m_gpuTime; }

        inline bool AllowsCommand(ShaderProgramType type) const { return m_shaderContext.program && m_shaderContext.program->m_description->type == type; }
        inline bool AllowsGraphicsCommand() const { return AllowsCommand(ShaderProgram_Graphics); }
        inline bool AllowsComputeCommand() const { return AllowsCommand(ShaderProgram_Compute); }

    private:

        render::RenderTargetBlendState& GetPsoBlendStateAttachment(uint32_t attachment);


        void InitScreenQuad();
        void DestroyScreenQuad();

        void CopyToPresentRt(render::TextureHandle texture);
        const render::RenderTargetHandle& GetPresentRt() const { check(m_swapchainIndex < (uint32_t)m_presentRts.size()); return m_presentRts[m_swapchainIndex]; }

        void ImGuiDraw();
        void FlushBeforeDraw();
        void FlushBeforeDispatch();
        void FlushMemoryContext();
        void ResolveBindings(render::BindingSetVector& bindingSetVector, render::BindingLayoutArray& bindingLayoutArray);
        render::GraphicsPipelineHandle GetPso(const render::GraphicsPipelineDescription& psoDesc, render::RenderTargetHandle rt);
        render::ComputePipelineHandle GetPso(const render::ComputePipelineDescription& psoDesc);

        void SetTextureSlot(const render::TextureHandle& texture, uint32_t set, uint32_t binding, uint32_t index, render::ImageLayout layout);

        inline ShaderMemoryContext* GetMemoryContext() const 
        {
            check(m_memoryContextId != UINT32_MAX);
            ShaderMemoryContext* ctx = m_memoryPool->GetContext(m_memoryContextId);
            check(ctx);
            return ctx;
        }

        inline void CreateMemoryContext()
        {
            check(m_memoryContextId == UINT32_MAX);
            m_memoryContextId = m_memoryPool->CreateContext();
        }

        inline void SubmitMemoryContext(uint64_t submissionId)
        {
            m_memoryPool->Submit(submissionId, &m_memoryContextId, 1);
        }
        
        void ImGuiDrawGpuProfiler();

    private:
        // Device context. Communication with render api.
        render::Device* m_device;
        uint32_t m_swapchainIndex;
        Mist::tCircularBuffer<uint32_t, 6> m_swapchainHistoric;
        // Frame counter
        uint64_t m_frame;
        // Store transform block data 
        HeapArray<glm::mat4> m_transforms;

        render::Extent2D m_renderResolution;
        render::Extent2D m_backbufferResolution;
        render::CommandStats m_cmdStats;

        // Syncronization objects. One per frame in flight.
        struct FrameSyncContext
        {
            uint32_t count = 0;
            render::SemaphoreHandle* renderQueueSemaphores = nullptr;
            render::SemaphoreHandle* presentSemaphores = nullptr;
            uint64_t* presentSubmission = nullptr;
            FrameResourceTrack* frameResources = nullptr;
            GpuFrameProfiler* timestampQueries = nullptr;

            void Init(render::Device* device, uint32_t frameCount);
            void Destroy();

        } m_frameSyncronization;
        inline const render::SemaphoreHandle& GetPresentSemaphore() const { return m_frameSyncronization.presentSemaphores[m_frame % m_frameSyncronization.count]; }
        inline const render::SemaphoreHandle& GetRenderSemaphore() const { check(m_swapchainIndex < m_frameSyncronization.count); return m_frameSyncronization.renderQueueSemaphores[m_swapchainIndex]; }
        inline uint64_t GetPresentSubmissionId() const { return m_frameSyncronization.presentSubmission[m_frame % m_frameSyncronization.count]; }
        inline uint64_t SetPresentSubmissionId(uint64_t submission) { return m_frameSyncronization.presentSubmission[m_frame % m_frameSyncronization.count] = submission; }
        inline FrameResourceTrack& GetFrameResources() { return m_frameSyncronization.frameResources[m_frame % m_frameSyncronization.count]; }
		inline GpuFrameProfiler& GetFrameProfiler() { return m_frameSyncronization.timestampQueries[m_frame % m_frameSyncronization.count]; }

        // Command list with transfer, graphics and compute commands
        GraphicsPipelineContext m_graphicsContext;
        ComputePipelineContext m_computeContext;
        ShaderContext m_shaderContext;
		render::CommandListHandle m_cmd;
		uint32_t m_memoryContextId; 

        // Render targets and other resources.
        render::TextureHandle m_ldrTexture;
        render::TextureHandle m_depthTexture;
        render::RenderTargetHandle m_ldrRt;
        render::TextureHandle m_defaultTexture;
        // Present render targets
        Mist::tDynArray<render::RenderTargetHandle> m_presentRts;

        // Screen quad draw call resources
        struct
        {
            ShaderProgram* shader;
            render::BufferHandle ib;
            render::BufferHandle vb;
            render::SamplerHandle sampler;
        } m_screenQuadCopy;

        // Memory and cache management.
        Mist::tMap<render::GraphicsPipelineDescription, render::GraphicsPipelineHandle> m_graphicsPsoMap;
        Mist::tMap<render::ComputePipelineDescription, render::ComputePipelineHandle> m_computePsoMap;
        BindingCache* m_bindingCache;
        SamplerCache* m_samplerCache;
        ShaderMemoryPool* m_memoryPool;
        // optimization to keep allocated memory in FlushBeforeDraw()
        render::BindingSetDescription m_bindingDesc;

        ShaderDb m_shaderDb;
        double m_gpuTime;
        GpuFrameProfiler::QueryTree m_lastQueryTree;
    };
}

/**
 * hash functions
 */

namespace std
{
    template <>
    struct hash<render::shader_compiler::CompileMacroDefinition>
    {
        size_t operator()(const render::shader_compiler::CompileMacroDefinition& options) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, options.macro.CStr());
            Mist::HashCombine(seed, options.value.CStr());
            return seed;
        }
    };

    template <>
    struct hash<render::shader_compiler::CompilationOptions>
    {
        size_t operator()(const render::shader_compiler::CompilationOptions& options) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, string(options.entryPoint));
            for (uint32_t i = 0; i < (uint32_t)options.macroDefinitionArray.size(); ++i)
                Mist::HashCombine(seed, options.macroDefinitionArray[i]);
            return seed;
        }
    };

    template <>
    struct hash<rendersystem::ShaderFileDescription>
    {
        size_t operator()(const rendersystem::ShaderFileDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.filePath);
            Mist::HashCombine(seed, desc.options);
            return seed;
        }
    };

    template <>
    struct hash<rendersystem::ShaderDynamicBufferDescription>
    {
        size_t operator()(const rendersystem::ShaderDynamicBufferDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.name);
            Mist::HashCombine(seed, desc.count);
            return seed;
        }
    };

    template <>
    struct hash<rendersystem::ShaderBuildDescription>
    {
        size_t operator()(const rendersystem::ShaderBuildDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.type);
            switch (desc.type)
            {
            case rendersystem::ShaderProgram_Graphics:
                Mist::HashCombine(seed, desc.vsDesc);
                Mist::HashCombine(seed, desc.fsDesc);
                break;
            case rendersystem::ShaderProgram_Compute:
                Mist::HashCombine(seed, desc.csDesc);
                break;
            default:
                unreachable_code();
                break;
            }
            for (uint32_t i = 0; i < (uint32_t)desc.dynamicBuffers.size(); ++i)
                Mist::HashCombine(seed, desc.dynamicBuffers[i]);
            return seed;
        }
    };
}


namespace rendersystem
{
    class ShaderProgramCache
    {
    public:
        ShaderProgram* GetOrCreateShader(render::Device* device, const ShaderBuildDescription& desc);
    private:
        Mist::tMap<ShaderBuildDescription, ShaderProgram*> m_shaders;
    };
}
