#pragma once

#ifdef RBE_VK
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#endif


#include "Core/Types.h"
#include "Types.h"

// declare first the structs which are used in the hash functions
namespace render
{
    struct BufferDescription;
    struct CommandBuffer;

    struct BufferRange
    {
        size_t offset = 0;
        size_t size = 0;

        BufferRange() = default;
        BufferRange(size_t _offset, size_t _size) : offset(_offset), size(_size) {}
        ~BufferRange() {}

        BufferRange Resolve(const BufferDescription& description) const;

        inline bool operator==(const BufferRange& other) const
        {
            return offset == other.offset && size == other.size;
        }

        inline bool operator!=(const BufferRange& other) const
        {
            return !(*this == other);
        }

        inline bool IsWholeBuffer() const { return offset == 0 && size == UINT32_MAX; }
        static BufferRange WholeBuffer() { return { 0, UINT32_MAX }; }
    };

    struct BufferDescription
    {
        size_t size;
        BufferUsage bufferUsage;
        MemoryUsage memoryUsage;

        inline bool operator==(const BufferDescription& other) const
        {
            return size == other.size &&
                bufferUsage == other.bufferUsage &&
                memoryUsage == other.memoryUsage;
        }

        inline bool operator!=(const BufferDescription& other) const
        {
            return !(*this == other);
        }
    };

    struct TextureDescription
    {
        Format format;
        uint32_t mipLevels = 1;
        uint32_t layers = 1;
        ImageUsage usage;
        ImageDimension dimension = ImageDimension_2D;
        ImageLayout initialLayout = ImageLayout_Undefined;
        Extent3D extent = {0,0,0};
        MemoryUsage memoryUsage;

        inline bool operator==(const TextureDescription& other) const
        {
            return format == other.format &&
                mipLevels == other.mipLevels &&
                layers == other.layers &&
                usage == other.usage &&
                dimension == other.dimension &&
                initialLayout == other.initialLayout &&
                extent == other.extent &&
                memoryUsage == other.memoryUsage;
        }

        inline bool operator!=(const TextureDescription& other) const
        {
            return !(*this == other);
        }
    };

    struct TextureSubresourceRange
    {
        static constexpr uint32_t AllMipLevels = UINT32_MAX;
        static constexpr uint32_t AllLayers = UINT32_MAX;

        uint32_t baseMipLevel = 0;
        uint32_t countMipLevels = 1;
        uint32_t baseLayer = 0;
        uint32_t countLayers = 1;

        TextureSubresourceRange() = default;
        constexpr TextureSubresourceRange(uint32_t _baseMipLevel, uint32_t _countMipLevels, uint32_t _baseLayer, uint32_t _countLayers)
            : baseMipLevel(_baseMipLevel),
            countMipLevels(_countMipLevels),
            baseLayer(_baseLayer),
            countLayers(_countLayers) {
        }
        ~TextureSubresourceRange() {}

        TextureSubresourceRange Resolve(const TextureDescription& description, bool singleMipLevel = false) const;

        static TextureSubresourceRange AllSubresources() { return { 0,TextureSubresourceRange::AllMipLevels, 0, TextureSubresourceRange::AllLayers }; }

        inline bool operator==(const TextureSubresourceRange& other) const
        {
            return baseMipLevel == other.baseMipLevel &&
                countMipLevels == other.countMipLevels &&
                baseLayer == other.baseLayer &&
                countLayers == other.countLayers;
        }
        inline bool operator!=(const TextureSubresourceRange& other) const
        {
            return !(*this == other);
        }
    };

    struct TextureViewDescription
    {
        TextureSubresourceRange range = TextureSubresourceRange::AllSubresources();
        ImageDimension dimension = ImageDimension_Undefined;
        Format format = Format_Undefined;

        inline bool operator==(const TextureViewDescription& other) const
        {
            return range == other.range &&
                dimension == other.dimension &&
                format == other.format;
        }

        inline bool operator!=(const TextureViewDescription& other) const
        {
            return !(*this == other);
        }
    };

    struct  SamplerDescription
    {
        Filter minFilter = Filter_Linear;
        Filter magFilter = Filter_Linear;
        Filter mipmapMode = Filter_Linear;
        SamplerAddressMode addressModeU = SamplerAddressMode_ClampToEdge;
        SamplerAddressMode addressModeV = SamplerAddressMode_ClampToEdge;
        SamplerAddressMode addressModeW = SamplerAddressMode_ClampToEdge;
        float mipLodBias = 0.f;
        float minLod = 0.f;
        float maxLod = 0.f;
        float maxAnisotropy = 1.f;

        inline bool operator==(const SamplerDescription& other) const
        {
            return minFilter == other.minFilter &&
                magFilter == other.magFilter &&
                mipmapMode == other.mipmapMode &&
                addressModeU == other.addressModeU &&
                addressModeV == other.addressModeV &&
                addressModeW == other.addressModeW &&
                mipLodBias == other.mipLodBias &&
                minLod == other.minLod &&
                maxLod == other.maxLod &&
                maxAnisotropy == other.maxAnisotropy;
        }

        inline bool operator!=(const SamplerDescription& other) const
        {
            return !(*this == other);
        }
    };
}

// hash functions of custom structs
namespace std
{
    template <>
    struct hash<render::BufferDescription>
    {
        size_t operator()(const render::BufferDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.size);
            Mist::HashCombine(seed, desc.bufferUsage);
            Mist::HashCombine(seed, desc.memoryUsage);
            return seed;
        }
    };

    template <>
    struct hash<render::TextureDescription>
    {
        size_t operator()(const render::TextureDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.format);
            Mist::HashCombine(seed, desc.extent.width);
            Mist::HashCombine(seed, desc.extent.height);
            Mist::HashCombine(seed, desc.extent.depth);
            Mist::HashCombine(seed, desc.mipLevels);
            Mist::HashCombine(seed, desc.layers);
            Mist::HashCombine(seed, desc.usage);
            Mist::HashCombine(seed, desc.dimension);
            return seed;
        }
    };

    template <>
    struct hash<render::TextureSubresourceRange>
    {
        size_t operator()(const render::TextureSubresourceRange& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.baseMipLevel);
            Mist::HashCombine(seed, desc.countMipLevels);
            Mist::HashCombine(seed, desc.baseLayer);
            Mist::HashCombine(seed, desc.countLayers);
            return seed;
        }
    };

    template <>
    struct hash<render::TextureViewDescription>
    {
        size_t operator()(const render::TextureViewDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.range);
            Mist::HashCombine(seed, desc.dimension);
            Mist::HashCombine(seed, desc.format);
            return seed;
        }
    };

    template <>
    struct hash<render::SamplerDescription>
    {
        size_t operator()(const render::SamplerDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.minFilter);
            Mist::HashCombine(seed, desc.magFilter);
            Mist::HashCombine(seed, desc.mipmapMode);
            Mist::HashCombine(seed, desc.addressModeU);
            Mist::HashCombine(seed, desc.addressModeV);
            Mist::HashCombine(seed, desc.addressModeW);
            Mist::HashCombine(seed, desc.mipLodBias);
            Mist::HashCombine(seed, desc.minLod);
            Mist::HashCombine(seed, desc.maxLod);
            Mist::HashCombine(seed, desc.maxAnisotropy);
            return seed;
        };
    };
}


namespace render
{
    class Device;

    /**
     * Vulkan context
     */

#ifdef RBE_VK
    typedef VmaAllocation Alloc;
    typedef VmaAllocator MemoryAllocator;
#endif

    struct MemoryStats
    {
        size_t allocationCounts;
        size_t currentAllocated;
        size_t maxAllocated;
    };

    struct MemoryContext
    {
        MemoryAllocator allocator;
        MemoryStats bufferStats;
        MemoryStats imageStats;
    };

    struct VulkanContext
    {
#ifdef RBE_VK
        VkInstance instance;
        VkDevice device;
        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkAllocationCallbacks* allocationCallbacks;
        MemoryContext memoryContext;

        VkPhysicalDeviceProperties physicalDeviceProperties;
        PFN_vkCmdBeginDebugUtilsLabelEXT pfn_vkCmdBeginDebugUtilsLabelEXT;
        PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT;
        PFN_vkCmdInsertDebugUtilsLabelEXT pfn_vkCmdInsertDebugUtilsLabelEXT;
        PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT;

        VulkanContext(VkInstance _instance,
            VkDevice _device,
            VkSurfaceKHR _surface,
            VkPhysicalDevice _physicalDevice,
            VkDebugUtilsMessengerEXT _debugMessenger,
            VkAllocationCallbacks* _allocationCallbacks)
            : instance(_instance), 
            device(_device), 
            physicalDevice(_physicalDevice),
            surface(_surface), 
            debugMessenger(_debugMessenger), 
            allocationCallbacks(_allocationCallbacks),
            memoryContext{ nullptr, {}, {} }
        {
            check(instance && device && physicalDevice && surface);

            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

            // Load external pfn
#define GET_VK_PROC_ADDRESS(fn) (PFN_##fn)vkGetInstanceProcAddr(instance, #fn)
            pfn_vkCmdBeginDebugUtilsLabelEXT = GET_VK_PROC_ADDRESS(vkCmdBeginDebugUtilsLabelEXT);
            pfn_vkCmdEndDebugUtilsLabelEXT = GET_VK_PROC_ADDRESS(vkCmdEndDebugUtilsLabelEXT);
            pfn_vkCmdInsertDebugUtilsLabelEXT = GET_VK_PROC_ADDRESS(vkCmdInsertDebugUtilsLabelEXT);
            pfn_vkSetDebugUtilsObjectNameEXT = GET_VK_PROC_ADDRESS(vkSetDebugUtilsObjectNameEXT);
#undef GET_VK_PROC_ADDRESS
        }
#endif
    };

    /**
     * Sync
     */

    class Semaphore final : public Mist::Ref<Semaphore>
    {
    public:
        Semaphore(Device* device);
        ~Semaphore();

        bool m_isTimeline;
        VkSemaphore m_semaphore;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<Semaphore> SemaphoreHandle;


    /**
     * Buffer
     */

    class Buffer final : public Mist::Ref<Buffer>
    {
    public:

        Buffer(Device* device) 
            : m_device(device),
            m_alloc(nullptr),
            m_buffer(VK_NULL_HANDLE),
            m_description{}
        {}
        ~Buffer();

        inline bool IsAllocated() const { return m_alloc != nullptr && m_buffer != VK_NULL_HANDLE; }

        Alloc m_alloc;
        VkBuffer m_buffer;
        BufferDescription m_description;

    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<Buffer> BufferHandle;

    /**
     * Texture
     */

    struct TextureView
    {
        TextureViewDescription m_description;

        VkImageView m_view;
        VkImage m_image;

        TextureView()
            : m_view(VK_NULL_HANDLE),
            m_image(VK_NULL_HANDLE)
        { }

    };

    class Texture final : public Mist::Ref<Texture>
    {
    public:

        Texture(Device* device) 
            : m_device(device),
            m_description{},
            m_alloc(nullptr),
            m_image(VK_NULL_HANDLE),
            m_owner(true)
        { }
        ~Texture();

        inline bool IsAllocated() const { return m_image != VK_NULL_HANDLE && (!m_owner || m_alloc != nullptr); }
        size_t GetImageSize() const;

        TextureView* GetView(const TextureViewDescription& viewDescription);

        TextureDescription m_description;
        Alloc m_alloc;
        VkImage m_image;
        bool m_owner;
        Mist::tMap<TextureViewDescription, TextureView> m_views;
        typedef Mist::tMap<TextureViewDescription, TextureView>::iterator ViewIterator;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<Texture> TextureHandle;

    /**
     * Sampler
     */

    class Sampler final : public Mist::Ref<Sampler>
    {
    public:

        Sampler(Device* device)
            : m_device(device),
            m_sampler(VK_NULL_HANDLE)
        { }
        ~Sampler();
        inline bool IsAllocated() const { return m_sampler != VK_NULL_HANDLE; }
        VkSampler m_sampler;
        SamplerDescription m_description;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<Sampler> SamplerHandle;

    /**
     * Shader
     */

    struct ShaderDescription
    {
        ShaderType type = ShaderType_None;
        Mist::tString name;
        Mist::tString entryPoint = "main";
    };

    class Shader final : public Mist::Ref<Shader>
    {
    public:
        Shader(Device* device)
            : m_device(device),
            m_shader(VK_NULL_HANDLE),
            m_description{}
        {
        }
        ~Shader();
        inline bool IsAllocated() const { return m_shader != VK_NULL_HANDLE; }
        VkShaderModule m_shader;
        ShaderDescription m_description;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<Shader> ShaderHandle;

    /**
     * Input layout
     */

    struct VertexInputAttribute
    {
        Format format = Format_Undefined;
        inline bool operator==(const VertexInputAttribute& other) const
        {
            return format == other.format;
        }
        inline bool operator!=(const VertexInputAttribute& other) const
        {
            return !(*this == other);
        }
    };

    struct VertexInputLayout
    {
        static constexpr uint32_t MaxAttributes = 16;
        
        Mist::tStaticArray<VkVertexInputAttributeDescription, MaxAttributes> m_attributes;
        VkVertexInputBindingDescription m_binding;

        VertexInputLayout() : m_attributes{}, m_binding{} {}
        static VertexInputLayout BuildVertexInputLayout(const VertexInputAttribute* attributes, uint32_t count);
    };


    /**
     * Render target
     */


    struct RenderTargetAttachment
    {
        TextureHandle texture = nullptr;
        TextureSubresourceRange range = TextureSubresourceRange(0,1,0,1);
        Format format;
        bool isReadOnly = false;

        inline bool IsValid() const
        {
            return texture && texture->IsAllocated();
        }
    };

    struct RenderTargetDescription
    {
        static constexpr uint32_t MaxRenderAttachments = 8;
        Mist::tStaticArray<RenderTargetAttachment, MaxRenderAttachments> colorAttachments;
        RenderTargetAttachment depthStencilAttachment;

        RenderTargetDescription& AddColorAttachment(TextureHandle texture, const TextureSubresourceRange& range, Format format);
        RenderTargetDescription& SetDepthStencilAttachment(TextureHandle texture, const TextureSubresourceRange& range, Format format);
    };

	struct RenderTargetInfo
	{
		Mist::tStaticArray<Format, RenderTargetDescription::MaxRenderAttachments> colorFormats;
		Format depthStencilFormat = Format_Undefined;
		Extent2D extent;
	};

    class RenderTarget final : public Mist::Ref<RenderTarget>
    {
    public:

        RenderTarget(Device* device)
            : m_device(device),
            m_renderPass(VK_NULL_HANDLE),
            m_framebuffer(VK_NULL_HANDLE)
        {
        }
        ~RenderTarget();
        RenderTargetInfo GetInfo() const;

        void BeginPass(CommandBuffer* cmd, Rect2D renderArea);
        void BeginPass(CommandBuffer* cmd);
        void EndPass(CommandBuffer* cmd);

        void ClearColor(CommandBuffer* cmd, float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);
        void ClearDepthStencil(CommandBuffer* cmd, float depth = 1.f, uint32_t stencil = 0);

        VkRenderPass m_renderPass;
        VkFramebuffer m_framebuffer;
        RenderTargetDescription m_description;
        RenderTargetInfo m_info;

    private:

        Device* m_device;
    };
    typedef Mist::RefPtr<RenderTarget> RenderTargetHandle;

    /**
     * Render states
     */

    struct RenderTargetBlendState
    {
        bool blendEnable = false;
        BlendFactor srcBlend = BlendFactor_One;
        BlendFactor dstBlend = BlendFactor_Zero;
        BlendOp blendOp = BlendOp_Add;
        BlendFactor srcAlphaBlend = BlendFactor_One;
        BlendFactor dstAlphaBlend = BlendFactor_Zero;
        BlendOp alphaBlendOp = BlendOp_Add;
        ColorMask colorWriteMask = ColorMask_All; // RGBA

        inline bool operator==(const RenderTargetBlendState& other) const
        {
            return blendEnable == other.blendEnable &&
                srcBlend == other.srcBlend &&
                dstBlend == other.dstBlend &&
                blendOp == other.blendOp &&
                srcAlphaBlend == other.srcAlphaBlend &&
                dstAlphaBlend == other.dstAlphaBlend &&
                alphaBlendOp == other.alphaBlendOp &&
                colorWriteMask == other.colorWriteMask;
        }

        inline bool operator!=(const RenderTargetBlendState& other) const
        {
            return !(*this == other);
        }
    };

    struct BlendState
    {
        Mist::tStaticArray<RenderTargetBlendState, RenderTargetDescription::MaxRenderAttachments> renderTargetBlendStates;

        inline bool operator==(const BlendState& other) const
        {
            if (this == &other)
                return true;
            if (renderTargetBlendStates.GetSize() != other.renderTargetBlendStates.GetSize())
                return false;
            for (uint32_t i = 0; i < RenderTargetDescription::MaxRenderAttachments; ++i)
            {
                if (renderTargetBlendStates[i] != other.renderTargetBlendStates[i])
                    return false;
            }
            return true;
        }

        inline bool operator!=(const BlendState& other) const
        {
            return !(*this == other);
        }
    };

    struct StencilOpState
    {
        StencilOp failOp = StencilOp_Keep;
        StencilOp depthFailOp = StencilOp_Keep;
        StencilOp passOp = StencilOp_Keep;
        CompareOp compareOp = CompareOp_Always;

        inline bool operator==(const StencilOpState& other) const
        {
            return failOp == other.failOp &&
                depthFailOp == other.depthFailOp &&
                passOp == other.passOp &&
                compareOp == other.compareOp;
        }

        inline bool operator!=(const StencilOpState& other) const
        {
            return !(*this == other);
        }
    };

    struct DepthStencilState
    {
        bool depthTestEnable = false;
        bool depthWriteEnable = false;
        CompareOp depthCompareOp = CompareOp_Less;
        bool stencilTestEnable = false;
        uint8_t stencilReadMask = 0xFF;
        uint8_t stencilWriteMask = 0xFF;
        uint8_t stencilRefValue = 0;
        StencilOpState frontFace;
        StencilOpState backFace;

        inline bool operator==(const DepthStencilState& other) const
        {
            return depthTestEnable == other.depthTestEnable &&
                depthWriteEnable == other.depthWriteEnable &&
                depthCompareOp == other.depthCompareOp &&
                stencilTestEnable == other.stencilTestEnable &&
                stencilReadMask == other.stencilReadMask &&
                stencilWriteMask == other.stencilWriteMask &&
                stencilRefValue == other.stencilRefValue &&
                frontFace == other.frontFace &&
                backFace == other.backFace;
        }

        inline bool operator!=(const DepthStencilState& other) const
        {
            return !(*this == other);
        }
    };

    struct RasterState
    {
        RasterFillMode fillMode = RasterFillMode_Fill;
        RasterCullMode cullMode = RasterCullMode_Back;
        bool frontFaceCounterClockWise = false;
        bool depthBiasEnable = false;
        float depthBiasConstantFactor = 0.f;
        float depthBiasClamp = 0.f;
        float depthBiasSlopeFactor = 0.f;
        bool depthClampEnable = false;
        bool scissorEnable = false;

        inline bool operator==(const RasterState& other) const
        {
            return fillMode == other.fillMode &&
                cullMode == other.cullMode &&
                frontFaceCounterClockWise == other.frontFaceCounterClockWise &&
                depthBiasEnable == other.depthBiasEnable &&
                depthBiasConstantFactor == other.depthBiasConstantFactor &&
                depthBiasClamp == other.depthBiasClamp &&
                depthBiasSlopeFactor == other.depthBiasSlopeFactor &&
                depthClampEnable == other.depthClampEnable &&
                scissorEnable == other.scissorEnable;
        }

        inline bool operator!=(const RasterState& other) const
        {
            return !(*this == other);
        }
    };

    struct ViewportState
    {
        Viewport viewport;
        Rect scissor;
    };

    struct RenderState
    {
        BlendState blendState;
        DepthStencilState depthStencilState;
        RasterState rasterState;
        ViewportState viewportState;

        inline bool operator==(const RenderState& other) const
        {
            return blendState == other.blendState &&
                depthStencilState == other.depthStencilState &&
                rasterState == other.rasterState;
        }
        inline bool operator!=(const RenderState& other) const
        {
            return !(*this == other);
        };
    };

    /************************************************************************/
    /* Bindings                                                             */
    /************************************************************************/

    struct BindingLayoutItem
    {
        ResourceType type = ResourceType_None;
        uint32_t binding = 0;
        uint32_t size = 0;

        BindingLayoutItem() = default;
        BindingLayoutItem(ResourceType _type, uint32_t _binding, uint32_t _size)
            : type(_type),
            binding(_binding),
            size(_size)
        { }

        inline bool operator==(const BindingLayoutItem& other) const
        {
            return type == other.type &&
                binding == other.binding &&
                size == other.size;
        }

        inline bool operator!=(const BindingLayoutItem& other) const
        {
            return !(*this == other);
        }
    };

    struct BindingLayoutDescription
    {
        static constexpr uint32_t MaxBindings = 16;
        Mist::tStaticArray<BindingLayoutItem, MaxBindings> bindings;
        uint32_t setIndex = 0;
        ShaderType shaderType = ShaderType_None;

        BindingLayoutDescription& SetShaderType(ShaderType type) { shaderType = type; return *this; }
        BindingLayoutDescription& PushTextureSRV() { bindings.Push(BindingLayoutItem(ResourceType_TextureSRV, bindings.GetSize(), 0)); return *this; }
        BindingLayoutDescription& PushTextureUAV() { bindings.Push(BindingLayoutItem(ResourceType_TextureUAV, bindings.GetSize(), 0)); return *this; }
        BindingLayoutDescription& PushConstantBuffer(size_t size) { bindings.Push(BindingLayoutItem(ResourceType_ConstantBuffer, bindings.GetSize(), size)); return *this; }
        BindingLayoutDescription& PushVolatileConstantBuffer(size_t size) { bindings.Push(BindingLayoutItem(ResourceType_VolatileConstantBuffer, bindings.GetSize(), size)); return *this; }
        BindingLayoutDescription& PushBufferUAV(size_t size) { bindings.Push(BindingLayoutItem(ResourceType_BufferUAV, bindings.GetSize(), size)); return *this; }

        inline bool operator==(const BindingLayoutDescription& other) const
        {
            if (bindings.GetSize() != other.bindings.GetSize())
                return false;
            if (setIndex != other.setIndex || shaderType != other.shaderType)
                return false;
            for (uint32_t i = 0; i < bindings.GetSize(); ++i)
            {
                if (bindings[i] != other.bindings[i])
                    return false;
            }
            return true;
        }

        inline bool operator!=(const BindingLayoutDescription& other) const
        {
            return !(*this == other);
        }
    };

    class BindingLayout final : public Mist::Ref<BindingLayout>
    {
    public:
        static constexpr uint32_t MaxLayouts = 8;
        BindingLayout(Device* device)
            : m_device(device),
            m_description{},
            m_layout(VK_NULL_HANDLE)
        {
        }
        ~BindingLayout();
        inline bool IsAllocated() const { return m_layout != VK_NULL_HANDLE; }
        VkDescriptorSetLayout m_layout;
        BindingLayoutDescription m_description;
        Mist::tStaticArray<VkDescriptorPoolSize, 8> m_poolSizes;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<BindingLayout> BindingLayoutHandle;
    typedef Mist::tStaticArray<BindingLayoutHandle, BindingLayout::MaxLayouts> BindingLayoutArray;

    void CreatePipelineLayout(Device* device, const BindingLayoutArray& bindingLayouts,
        VkPipelineLayout& pipelineLayout);

    struct BindingSetItem
    {
        static constexpr uint32_t MaxBindingSets = 32;

        // Resource can be buffer or texture or sampler
        union
        {
            Buffer* buffer;
            Texture* texture;
            void* resource;
        };
        // Sampler handle used for TextureSRV
        SamplerHandle sampler;

        uint32_t binding;
        ResourceType type;
        ImageDimension dimension;
        
        union
        {
            TextureSubresourceRange textureSubresources;
            BufferRange bufferRange;
        };

        BindingSetItem() {}
        ~BindingSetItem() {}

        static BindingSetItem CreateTextureSRVItem(uint32_t slot, Texture* texture, SamplerHandle sampler,
            TextureSubresourceRange subresource = TextureSubresourceRange::AllSubresources(), ImageDimension dimension = ImageDimension_Undefined);
        static BindingSetItem CreateTextureUAVItem(uint32_t slot, Texture* texture,
            TextureSubresourceRange subresource = {0,1,0,TextureSubresourceRange::AllLayers}, ImageDimension dimension = ImageDimension_Undefined);
        static BindingSetItem CreateConstantBufferItem(uint32_t slot, Buffer* buffer, BufferRange bufferRange = BufferRange::WholeBuffer());
        static BindingSetItem CreateVolatileConstantBufferItem(uint32_t slot, Buffer* buffer, BufferRange bufferRange = BufferRange::WholeBuffer());
        static BindingSetItem CreateBufferUAVItem(uint32_t slot, Buffer* buffer, BufferRange bufferRange = BufferRange::WholeBuffer());

        inline bool operator==(const BindingSetItem& other) const
        {
            return resource == other.resource &&
                binding == other.binding &&
                type == other.type &&
                (textureSubresources == other.textureSubresources || bufferRange == other.bufferRange);
        }

        inline bool operator!=(const BindingSetItem& other) const
        {
            return !(*this == other);
        }
    };

    typedef Mist::tStaticArray<BindingSetItem, BindingSetItem::MaxBindingSets> BindingSetItemArray;

    struct BindingSetDescription
    {
        BindingSetItemArray bindingItems;

        BindingSetDescription()
        { }

        BindingSetDescription& PushTextureSRV(uint32_t slot, Texture* texture, SamplerHandle sampler, TextureSubresourceRange subresource = TextureSubresourceRange::AllSubresources(), ImageDimension dimension = ImageDimension_Undefined)
        {
            bindingItems.Push(BindingSetItem::CreateTextureSRVItem(slot, texture, sampler, subresource, dimension));
            return *this;
        }

        BindingSetDescription& PushTextureUAV(uint32_t slot, Texture* texture, TextureSubresourceRange subresource = { 0,1,0,TextureSubresourceRange::AllLayers }, ImageDimension dimension = ImageDimension_Undefined)
        {
            bindingItems.Push(BindingSetItem::CreateTextureUAVItem(slot, texture, subresource, dimension));
            return *this;
        }

        BindingSetDescription& PushConstantBuffer(uint32_t slot, Buffer* buffer, BufferRange bufferRange = BufferRange::WholeBuffer())
        {
            bindingItems.Push(BindingSetItem::CreateConstantBufferItem(slot, buffer, bufferRange));
            return *this;
        }

        BindingSetDescription& PushVolatileConstantBuffer(uint32_t slot, Buffer* buffer, BufferRange bufferRange = BufferRange::WholeBuffer())
        {
            bindingItems.Push(BindingSetItem::CreateVolatileConstantBufferItem(slot, buffer, bufferRange));
            return *this;
        }

        BindingSetDescription& PushBufferUAV(uint32_t slot, Buffer* buffer, BufferRange bufferRange = BufferRange::WholeBuffer())
        {
            bindingItems.Push(BindingSetItem::CreateBufferUAVItem(slot, buffer, bufferRange));
            return *this;
        }

        inline bool operator==(const BindingSetDescription& other) const
        {
            if (bindingItems.GetSize() != other.bindingItems.GetSize())
                return false;
            for (uint32_t i = 0; i < bindingItems.GetSize(); ++i)
            {
                if (bindingItems[i] != other.bindingItems[i])
                    return false;
            }
            return true;
        }

        inline bool operator!=(const BindingSetDescription& other) const
        {
            return !(*this == other);
        }
    };

    class BindingSet final : public Mist::Ref<BindingSet>
    {
    public:
        static constexpr uint32_t MaxBindingSets = 8;
        BindingSet(Device* device)
            : m_device(device),
            m_description(),
            m_set(VK_NULL_HANDLE)
        {
        }
        
        ~BindingSet();

        inline bool IsAllocated() const { return m_set != VK_NULL_HANDLE; }

        VkDescriptorSet m_set;
        VkDescriptorPool m_pool;
        BindingSetDescription m_description;
        BindingLayoutHandle m_layout;

        // Resource tracking
        Mist::tDynArray<BufferHandle> m_buffers;
        Mist::tDynArray<TextureHandle> m_textures;
        Mist::tDynArray<SamplerHandle> m_samplers;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<BindingSet> BindingSetHandle;
    typedef Mist::tStaticArray<BindingSetHandle, BindingSet::MaxBindingSets> BindingSetArray;

    /**
     * Pipelines
     */

    struct GraphicsPipelineDescription
    {
        PrimitiveType primitiveType = PrimitiveType_TriangleList;
        ShaderHandle vertexShader;
        ShaderHandle fragmentShader;
        RenderState renderState;
        VertexInputLayout vertexInputLayout;
        BindingLayoutArray bindingLayouts;
    };

	class GraphicsPipeline : public Mist::Ref<GraphicsPipeline>
	{
	public:
		GraphicsPipeline(Device* device)
		: m_device(device) 
		{}
		~GraphicsPipeline();

        void UsePipeline(CommandBuffer* cmd);

		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		GraphicsPipelineDescription m_description;

	private:
		Device* m_device;
	};
    typedef Mist::RefPtr<GraphicsPipeline> GraphicsPipelineHandle;

    struct ComputePipelineDescription
    {
        ShaderHandle computeShader;
        BindingLayoutArray bindingLayouts;
    };

    class ComputePipeline : public Mist::Ref<ComputePipeline>
    {
    public:
        ComputePipeline(Device* device)
            : m_device(device)
        {
        }
        ~ComputePipeline();
        VkPipeline m_pipeline;
        VkPipelineLayout m_pipelineLayout;
        ComputePipelineDescription m_description;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<ComputePipeline> ComputePipelineHandle;

    /************************************************************************/
    /* Command queues and buffers                                           */
    /************************************************************************/

    uint32_t FindFamilyQueueIndex(Device* device, QueueType type);

    struct CommandBuffer
    {
        VkCommandBuffer cmd;
        VkCommandPool pool;
        QueueType type;
        uint32_t submissionId;

        void Begin();
        void End();
    };

    class CommandQueue
    {
    public:
        CommandQueue(Device* device, QueueType type);
        ~CommandQueue();

        CommandBuffer* CreateCommandBuffer();

        uint64_t SubmitCommandBuffers(CommandBuffer* const* cmds, uint32_t count);

        void AddWaitSemaphore(SemaphoreHandle semaphore, uint64_t value);
        void AddSignalSemaphore(SemaphoreHandle semaphore, uint64_t value);
        void AddWaitQueue(const CommandQueue* queue, uint64_t value);

        uint64_t QueryTrackingId();
        void ProcessInFlightCommands();

        // Returns true if there is the last finished submission id is greater than submissionId, so submissionId is finished.
        bool PollCommandSubmission(uint64_t submissionId);
        bool WaitForCommandSubmission(uint64_t submissionId, uint64_t timeout = 1);

        inline uint64_t GetLastSubmissionId() const { return m_submissionId; }
        inline uint32_t GetLastSubmissionIdFinished() const { return m_lastSubmissionIdFinished; }
        inline QueueType GetQueueType() const { return m_type; }

        VkQueue m_queue;
        uint32_t m_queueFamilyIndex;
        QueueType m_type;
    private:
        Device* m_device;


        uint64_t m_submissionId;
        uint64_t m_lastSubmissionIdFinished;

        SemaphoreHandle m_trackingSemaphore;

        Mist::tDynArray<CommandBuffer*> m_commandPool;
        Mist::tDynArray<CommandBuffer*> m_submittedCommandBuffers;
        Mist::tDynArray<CommandBuffer*> m_createdCommandBuffers;

        static constexpr uint32_t MaxSemaphores = 8;
        Mist::tStaticArray<SemaphoreHandle, MaxSemaphores> m_waitSemaphores;
        Mist::tStaticArray<uint64_t, MaxSemaphores> m_waitSemaphoreValues;
        Mist::tStaticArray<SemaphoreHandle, MaxSemaphores> m_signalSemaphores;
        Mist::tStaticArray<uint64_t, MaxSemaphores> m_signalSemaphoreValues;
    };

    struct GraphicsState
    {
        GraphicsPipelineHandle pipeline = nullptr;
        RenderTargetHandle rt = nullptr;

        BindingSetArray bindings;

        BufferHandle vertexBuffer = nullptr;
        BufferHandle indexBuffer = nullptr;

        inline bool operator ==(const GraphicsState& other) const
        {
            if (pipeline != other.pipeline
                || rt != other.rt
                || vertexBuffer != other.vertexBuffer
                || indexBuffer != other.indexBuffer)
                return false;
            if (bindings.GetSize() != other.bindings.GetSize())
                return false;
            for (uint32_t i = 0; i < bindings.GetSize(); ++i)
            {
                if (bindings[i] != other.bindings[i])
                    return false;
            }
            return true;
        }

        inline bool operator!=(const GraphicsState& other) const { return !(*this == other); }
    };

    struct ComputeState
    {
        ComputePipelineHandle pipeline = nullptr;
        BindingSetArray bindings;

        inline bool operator ==(const ComputeState& other) const 
        {
            if (pipeline != other.pipeline)
                return false;
            if (bindings.GetSize() != other.bindings.GetSize())
                return false;
            for (uint32_t i = 0; i < bindings.GetSize(); ++i)
            {
                if (bindings[i] != other.bindings[i])
                    return false;
            }
            return true;
        }

        inline bool operator!=(const ComputeState& other) const { return !(*this == other); }
    };

    struct TextureBarrier
    {
        TextureHandle texture;
        ImageLayout oldLayout;
        ImageLayout newLayout;
        TextureSubresourceRange subresources = TextureSubresourceRange::AllSubresources();
    };

    class TransferMemory
    {
    public:
        TransferMemory(Device* device, BufferHandle buffer, uint64_t pointer, uint64_t availableSize);
        ~TransferMemory();

        void MapMemory();
        void UnmapMemory();

        void Write(const void* data, size_t size, size_t srcOffset = 0, size_t dstOffset = 0);

        const BufferHandle m_buffer;
        const uint64_t m_pointer;
        const uint64_t m_availableSize;
    private:
        Device* m_device;
        void* m_dst;
    };

    class TransferMemoryPool
    {
        static constexpr uint32_t Alignment = 256;
        static constexpr uint32_t InvalidChunkIndex = UINT32_MAX;
    public:
        struct Chunk
        {
            BufferHandle buffer = nullptr;
            uint64_t version = 0;
            uint64_t pointer = 0;
        };

        TransferMemoryPool(Device* device, uint64_t defaultChunkSize);
 
        TransferMemory Suballocate(uint64_t size);
        void Submit(uint64_t submissionId);
    protected:
        Chunk CreateChunk(uint64_t size);
    private:
        Device* m_device;
        uint64_t m_defaultChunkSize;
        Mist::tDynArray<Chunk> m_pool;
        Mist::tDynArray<uint32_t> m_submittedChunkIndices;
        uint32_t m_currentChunkIndex;
    };

    class CommandList final : public Mist::Ref<CommandList>
    {
    public:

        CommandList(Device* device);
        ~CommandList();

        static uint64_t ExecuteCommandLists(CommandList* const* lists, uint32_t count);
        uint64_t ExecuteCommandList();

        void BeginRecording();
        inline bool IsRecording() const { return m_currentCommandBuffer != nullptr; }
        void EndRecording();

        // Graphics
        void SetGraphicsState(const GraphicsState& state);
        void ClearColor(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);
        void ClearDepthStencil(float depth = 1.f, uint32_t stencil = 0);
        void SetViewport(float x, float y, float width, float height, float minDepth = 0.f, float maxDepth = 1.f);
        void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);
        void BindVertexBuffer(BufferHandle vb);
        void BindIndexBuffer(BufferHandle ib);
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);

        // Compute
        void SetComputeState(const ComputeState& state);
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        // Texture barriers
        void SetTextureState(const TextureBarrier* barriers, uint32_t count);
        void SetTextureState(const TextureBarrier& barrier);

        // Transfer
        void WriteBuffer(BufferHandle buffer, const void* data, size_t size, size_t srcOffset = 0, size_t dstOffset = 0);
        void WriteTexture(TextureHandle texture, uint32_t mipLevel, uint32_t layer, const void* data, size_t dataSize);


        inline bool AllowsCommandType(QueueType type) const { return IsRecording() && m_currentCommandBuffer->type & type; }

    private:

        void EndRenderPass();
        void ClearState();
        void Submitted(uint64_t submissionId);

    private:
        Device* m_device;
        GraphicsState m_graphicsState;
        ComputeState m_computeState;
        CommandBuffer* m_currentCommandBuffer;
        bool m_dirtyBindings;

        TransferMemoryPool m_transferMemoryPool;
    };
    typedef Mist::RefPtr<CommandList> CommandListHandle;

    /**
     * Device
     */

    struct DeviceDescription
    {
        Mist::tString name;
        bool enableValidationLayer;
        const void* windowHandle;
    };

    class Device
    {
    public:
        struct Swapchain
        {
            uint32_t width;
            uint32_t height;
            ColorSpace colorSpace;
            Format format;
            VkSwapchainKHR swapchain = VK_NULL_HANDLE;
            Mist::tDynArray<TextureHandle> images;
        };

        Device(const DeviceDescription& description);
        ~Device();

        Device(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(const Device&) = delete;
        Device& operator=(Device&&) = delete;

        inline const VulkanContext& GetContext() const { return *m_context; }

        CommandQueue* GetCommandQueue(QueueType type);

        CommandListHandle CreateCommandList();
        void DestroyCommandList(CommandList* commandList);

        SemaphoreHandle CreateRenderSemaphore(bool timelineSemaphore = false);
        void DestroyRenderSemaphore(Semaphore* semaphore);
        uint64_t GetSemaphoreTimelineCounter(SemaphoreHandle semaphore);

        BufferHandle CreateBuffer(const BufferDescription& description);
        void DestroyBuffer(Buffer* buffer);
        void WriteBuffer(BufferHandle buffer, const void* data, size_t size, size_t srcOffset = 0, size_t dstOffset = 0);

        TextureHandle CreateTexture(const TextureDescription& description);
        TextureHandle CreateTextureFromNative(const TextureDescription& description, VkImage image);
        void DestroyTexture(Texture* texture);

        SamplerHandle CreateSampler(const SamplerDescription& description);
        void DestroySampler(Sampler* sampler);

        ShaderHandle CreateShader(const ShaderDescription& description, const void* binary, size_t binarySize);
        void DestroyShader(Shader* shader);

        RenderTargetHandle CreateRenderTarget(const RenderTargetDescription& description);
        void DestroyRenderTarget(RenderTarget* renderTarget);

        GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDescription& description, RenderTargetHandle rt);
        void DestroyGraphicsPipeline(GraphicsPipeline* pipeline);

        BindingLayoutHandle CreateBindingLayout(const BindingLayoutDescription& description);
        void DestroyBindingLayout(BindingLayout* bindingLayout);

        ComputePipelineHandle CreateComputePipeline(const ComputePipelineDescription& description);
        void DestroyComputePipeline(ComputePipeline* pipeline);

        BindingSetHandle CreateBindingSet(const BindingSetDescription& description, BindingLayoutHandle layout);
        void DestroyBindingSet(BindingSet* bindingSet);

        const Swapchain& GetSwapchain() const { return m_swapchain; }
        uint32_t AcquireSwapchainIndex(SemaphoreHandle semaphoreToBeSignaled);
        void Present(SemaphoreHandle semaphoreToWait);

    private:
        void InitContext(const DeviceDescription& description);
        void InitMemoryContext();
        void InitQueue();
        void InitSwapchain(uint32_t width, uint32_t height);
        void DestroyMemoryContext();
        void DestroyContext();
        void DestroyQueue();
        void DestroySwapchain();

    private:
        VulkanContext* m_context;
        // Currently only use one queue with all functionalities. Add separates queues only if needed.
        CommandQueue* m_queue;

        Swapchain m_swapchain;
        uint32_t m_swapchainIndex;
    };
}

