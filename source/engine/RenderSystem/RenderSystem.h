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
    public:
        ShaderProgram(render::Device* device, const ShaderBuildDescription& description);
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

    /**
     * fixed render pipeline
     * 
     * set material
     * - set shaders
     * - set textures: only write descriptors
     * - set buffers: 
     *  - write cpu buffers 
     */

    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord0;
        glm::vec2 texCoord1;
        glm::vec4 tangent;
        glm::vec4 vertexColor;
    };

    struct ScreenVertex
    {
        glm::vec3 position;
        glm::vec2 texCoords0;
        glm::vec2 texCoords1;
    };

    struct Primitive
    {
        uint32_t first;
        uint32_t count;
        uint32_t mesh;
        uint32_t material;
    };

    struct MaterialTextureMap
    {
        render::TextureHandle albedo;
        render::TextureHandle normal;
        render::TextureHandle metallicRoughness;
        render::TextureHandle emissive;
        render::TextureHandle occlusion;
    };

    struct Material
    {
        render::DepthStencilState depthStencilState;
        render::ShaderHandle vs;
        render::ShaderHandle fs;

        float albedo[4];
        float roughness;
        float metallic;
        float emissive[4];

        MaterialTextureMap textureMap;
    };

    struct Mesh
    {
        HeapArray<Primitive> primitives;
        render::BufferHandle vb;
        render::BufferHandle ib;
        render::VertexInputLayout inputLayout;

        void Release()
        {
            primitives.Release();
        }
    };

    struct Model
    {
        HeapArray<Mesh> meshes;
        HeapArray<Material> materials;

        void Release()
        {
            for (uint32_t i = 0; i < meshes.count; ++i)
                meshes.data[i].Release();
            meshes.Release();
            materials.Release();
        }
    };

    struct ModelInstance
    {
        uint32_t model = UINT32_MAX;
        glm::mat4 transform = glm::mat4(1.f);
    };


    struct DrawList
    {
        glm::mat4 view;
        glm::mat4 projection;

        HeapArray<Model> models;
        HeapArray<ModelInstance> modelInstances;

        void Release()
        {
            for (uint32_t i = 0; i < models.count; ++i)
                models.data[i].Release();
            modelInstances.Release();
        }
    };

    class RenderSystem
    {
        struct RenderContext
        {
            render::CommandListHandle cmd;
            uint32_t memoryContextId;
            render::GraphicsState graphicsState;
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

        
        /**
         * TODO: bindings, set shader constants and textures
         */
        void SetShader(ShaderProgram* shader);
        void SetTextureSlot(render::TextureHandle texture, uint32_t set, uint32_t binding = 0);
        void SetTextureSlot(const char* id, render::TextureHandle texture);
        void SetTextureSlot(const render::TextureHandle* textures, uint32_t count, uint32_t set, uint32_t binding = 0);
        void SetTextureSlot(const char* id, const render::TextureHandle* textures, uint32_t count);
        void SetSampler(render::SamplerHandle sampler, uint32_t set, uint32_t binding = 0, uint32_t samplerIndex = 0);
        void SetSampler(render::SamplerHandle* sampler, uint32_t count, uint32_t set, uint32_t binding = 0);
        void SetSampler(const char* id, render::SamplerHandle sample);
        void SetSampler(const char* id, render::SamplerHandle* sample, uint32_t count);
        void SetSampler(const char* id, render::Filter minFilter, render::Filter magFilter,
            render::SamplerAddressMode addressModeU,
            render::SamplerAddressMode addressModeV,
            render::SamplerAddressMode addressModeW,
            uint32_t samplerIndex = 0);
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

    private:

        render::RenderTargetBlendState& GetPsoBlendStateAttachment(uint32_t attachment);

        void DrawDrawList(const DrawList& list);

        void InitScreenQuad();
        void DestroyScreenQuad();

        void CopyToPresentRt(render::TextureHandle texture);

        void ImGuiDraw();
        void FlushBeforeDraw();
        

    private:
        // Device context. Communication with render api.
        render::Device* m_device;
        uint32_t m_swapchainIndex;
        // Frame counter
        uint64_t m_frame;
        // Testing draw list architecture
        DrawList m_drawList;
        // Store transform block data 
        HeapArray<glm::mat4> m_transforms;

        render::Extent2D m_renderResolution;
        render::Extent2D m_backbufferResolution;

        // Syncronization objects. One per frame in flight.
        struct
        {
            render::SemaphoreHandle renderQueueSemaphore;
            render::SemaphoreHandle presentSemaphore;
            uint64_t submission = 0;
        } m_frameSyncronization[8];

        // Command list with transfer, graphics and compute commands
        render::CommandListHandle m_cmd;
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
