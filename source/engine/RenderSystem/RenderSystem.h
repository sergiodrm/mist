#pragma once

#include "RenderAPI/Device.h"
#include <glm/glm.hpp>

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

    class ShaderMemoryPool
    {
        struct Chunk
        {
            render::BufferHandle buffer;
            uint64_t pointer;
            uint64_t submissionId;
        };
    public:
        struct MemoryAllocation
        {
            uint32_t chunkIndex;
            uint64_t pointer;
            uint64_t size;

            static MemoryAllocation InvalidAllocation() { return { UINT32_MAX, UINT32_MAX, UINT32_MAX }; }
            inline bool IsValid() const { return chunkIndex != UINT32_MAX && pointer != UINT32_MAX && size != UINT32_MAX; }
        };
        ShaderMemoryPool(render::Device* device, uint64_t minChunkSize = 1 << 16);
        ~ShaderMemoryPool();

        MemoryAllocation Allocate(const char* id, uint64_t size);
        MemoryAllocation Find(const char* id) const;
        void Write(const MemoryAllocation& allocation, const void* src, uint64_t size, uint64_t srcOffset = 0, uint64_t dstOffset = 0);
        void Submit(uint64_t submissionId);


        inline render::BufferHandle GetBuffer(uint32_t chunkIndex) const { return m_pool[chunkIndex].buffer; }

    private:
        uint32_t GetFreeChunk(uint64_t size);
        uint32_t CreateChunk(uint64_t size);

    private:

        render::Device* m_device;
        Mist::tDynArray<Chunk> m_pool;
        Mist::tDynArray<uint32_t> m_submitted;
        Mist::tDynArray<uint32_t> m_toSubmit;
        Mist::tMap<Mist::tString, MemoryAllocation> m_allocations;
        uint32_t m_currentChunk;
        uint64_t m_minChunkSize;
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
        Mist::tMap<Mist::tString, uint32_t> m_map;
    };

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
    public:

        void Init(IWindow* window);
        void Destroy();

        void BeginFrame();
        void Draw();
        void EndFrame();

        void SetViewProjection(const glm::mat4& view, const glm::mat4& projection);

        inline uint64_t GetFrameIndex() const { return m_frame % m_device->GetSwapchain().images.size(); }
    private:

        render::GraphicsPipelineHandle GetPso(const render::GraphicsPipelineDescription& psoDesc, render::RenderTargetHandle rt);
        void DrawDrawList(const DrawList& list);

        void InitScreenQuad();
        void DestroyScreenQuad();

        void CopyToPresentRt(render::TextureHandle texture);

        void ImGuiDraw();
        

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


        // Syncronization objects. One per frame in flight.
        struct
        {
            render::SemaphoreHandle renderQueueSemaphore;
            render::SemaphoreHandle presentSemaphore;
            uint64_t submission = 0;
        } m_frameSyncronization[8];

        // Command list with transfer, graphics and compute commands
        render::CommandListHandle m_cmd;

        // Render targets and other resources.
        render::TextureHandle m_ldrTexture;
        render::TextureHandle m_depthTexture;
        render::RenderTargetHandle m_ldrRt;
        // Present render targets
        Mist::tDynArray<render::RenderTargetHandle> m_presentRts;

        // Screen quad draw call resources
        struct
        {
            render::ShaderHandle vs;
            render::ShaderHandle fs;
            render::BufferHandle ib;
            render::BufferHandle vb;
            render::SamplerHandle sampler;
        } m_screenQuadCopy;

        // Memory and cache management.
        Mist::tMap<render::GraphicsPipelineDescription, render::GraphicsPipelineHandle> m_psoMap;
        BindingCache* m_bindingCache;
        ShaderMemoryPool* m_memoryPool;
    };
}