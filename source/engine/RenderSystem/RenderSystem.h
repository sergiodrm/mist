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
        uint32_t GetCacheSize() const { return m_samplers.size(); }
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
        struct RenderContext
        {
            render::CommandListHandle cmd;
            uint32_t memoryContextId;
            render::GraphicsState graphicsState;
            bool pendingClearColor;
            float clearColor[4];
            bool pendingClearDepthStencil;
            float clearDepth;
            uint32_t clearStencil;
        };
    public:
        static constexpr uint32_t MaxTextureSlots = 8;
        static constexpr uint32_t MaxTextureBindingsPerSlot = 8;
        static constexpr uint32_t MaxTextureArrayCount = 8;

        void Init(IWindow* window);
        void Destroy();

        void BeginFrame();
        void Draw();
        void EndFrame();

        void BeginMarker(const char* name, render::Color color = render::Color::White());
        void EndMarker();

        inline render::RenderTargetHandle GetLDRTarget() const { return m_ldrRt; }
        inline render::TextureHandle GetLDRTexture() const { return m_ldrTexture; }

        inline const render::Extent2D& GetRenderResolution() const { return m_renderResolution; }
        inline const render::Extent2D& GetBackbufferResolution() const { return m_backbufferResolution; }

        void SetViewProjection(const glm::mat4& view, const glm::mat4& projection);

        render::Device* GetDevice() const { return m_device; }
        ShaderProgram* CreateShader(const ShaderBuildDescription& desc);
        void DestroyShader(ShaderProgram** shader);
        void ReloadAllShaders();

        inline uint64_t GetFrameIndex() const { return m_frame % m_device->GetSwapchain().images.size(); }
        render::GraphicsPipelineHandle GetPso(const render::GraphicsPipelineDescription& psoDesc, render::RenderTargetHandle rt);
        render::BindingSetHandle GetBindingSet(const render::BindingSetDescription& desc);
        render::SamplerHandle GetSampler(const render::SamplerDescription& desc);
        render::SamplerHandle GetSampler(render::Filter minFilter, render::Filter magFilter, 
            render::SamplerAddressMode addressModeU,
            render::SamplerAddressMode addressModeV,
            render::SamplerAddressMode addressModeW);

        void SetDefaultState();
        void ClearState();
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
        void SetScissor(float x0, float x1, float y0, float y1);

        void SetFillMode(render::RasterFillMode mode = render::RasterFillMode_Fill);
        void SetCullMode(render::RasterCullMode mode = render::RasterCullMode_Back);
        void SetPrimitive(render::PrimitiveType type = render::PrimitiveType_TriangleList);

        
        /**
         * TODO: bindings, set shader constants and textures
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
            render::SamplerAddressMode addressModeU,
            render::SamplerAddressMode addressModeV,
            render::SamplerAddressMode addressModeW,
            uint32_t samplerIndex = 0);
        void SetSampler(render::Filter minFilter, render::Filter magFilter,
            render::SamplerAddressMode addressModeU,
            render::SamplerAddressMode addressModeV,
            render::SamplerAddressMode addressModeW,
            uint32_t set, uint32_t binding = 0, uint32_t samplerIndex = 0);
        void SetShaderProperty(const char* id, const void* param, uint64_t size);
        void SetTextureLayout(render::TextureHandle texture, render::ImageLayout layout);
        void SetTextureAsResourceBinding(render::TextureHandle texture);
        void SetTextureAsRenderTargetAttachment(render::TextureHandle texture);

        void SetRenderTarget(render::RenderTargetHandle rt);

        void SetVertexBuffer(render::BufferHandle vb);
        void SetIndexBuffer(render::BufferHandle ib);
        void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, uint32_t firstVertex = 0, uint32_t firstInstance = 0);

        ShaderMemoryPool* GetMemoryPool() const { return m_memoryPool; }

        // Utilities
        void DrawFullscreenQuad();
        void ClearColor(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);
        void ClearDepthStencil(float depth = 1.f, uint32_t stencil = 0);

        void DumpState();

    private:

        render::RenderTargetBlendState& GetPsoBlendStateAttachment(uint32_t attachment);


        void InitScreenQuad();
        void DestroyScreenQuad();

        void CopyToPresentRt(render::TextureHandle texture);
        const render::RenderTargetHandle& GetPresentRt() const { check(m_swapchainIndex < (uint32_t)m_presentRts.size()); return m_presentRts[m_swapchainIndex]; }

        void ImGuiDraw();
        void FlushBeforeDraw();
        

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

        // Syncronization objects. One per frame in flight.
        struct FrameSyncContext
        {
            static constexpr uint32_t Count = 8;
            render::SemaphoreHandle renderQueueSemaphores[Count];
            render::SemaphoreHandle presentSemaphores[Count];
            uint64_t presentSubmission[Count];
        } m_frameSyncronization;
        inline const render::SemaphoreHandle& GetPresentSemaphore() const { return m_frameSyncronization.presentSemaphores[m_frame % FrameSyncContext::Count]; }
        inline const render::SemaphoreHandle& GetRenderSemaphore() const { check(m_swapchainIndex < FrameSyncContext::Count); return m_frameSyncronization.renderQueueSemaphores[m_swapchainIndex]; }
        inline uint64_t GetPresentSubmissionId() const { return m_frameSyncronization.presentSubmission[m_frame % FrameSyncContext::Count]; }
        inline uint64_t SetPresentSubmissionId(uint64_t submission) { return m_frameSyncronization.presentSubmission[m_frame % FrameSyncContext::Count] = submission; }

        // Command list with transfer, graphics and compute commands
        RenderContext m_renderContext;

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
        Mist::tMap<render::GraphicsPipelineDescription, render::GraphicsPipelineHandle> m_psoMap;
        BindingCache* m_bindingCache;
        SamplerCache* m_samplerCache;
        ShaderMemoryPool* m_memoryPool;

        // Shader memory context and textures
        uint32_t m_memoryContextId;
        render::TextureHandle m_textureSlots[MaxTextureSlots][MaxTextureBindingsPerSlot][MaxTextureArrayCount];
        render::SamplerHandle m_samplerSlots[MaxTextureSlots][MaxTextureBindingsPerSlot][MaxTextureArrayCount];
        ShaderProgram* m_program;

        render::GraphicsPipelineDescription m_psoDesc;

        ShaderDb m_shaderDb;
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
