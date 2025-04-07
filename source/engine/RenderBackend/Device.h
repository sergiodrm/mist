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
    static constexpr TextureSubresourceRange TextureSubresourceWholeRange = { 0,TextureSubresourceRange::AllMipLevels, 0, TextureSubresourceRange::AllLayers };

    struct TextureViewDescription
    {
        TextureSubresourceRange range;
        ImageDimension dimension;
        Format format;

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
        float minLod;
        float maxLod;
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
            m_image(VK_NULL_HANDLE)
        { }
        ~Texture();

        inline bool IsAllocated() const { return m_alloc != nullptr && m_image != VK_NULL_HANDLE; }
        size_t GetImageSize() const;

        TextureView* GetView(const TextureViewDescription& viewDescription);

        TextureDescription m_description;
        Alloc m_alloc;
        VkImage m_image;
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


        VkRenderPass m_renderPass;
        VkFramebuffer m_framebuffer;
        RenderTargetDescription m_description;

    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<RenderTarget> RenderTargetHandle;

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

        Device(const DeviceDescription& description);
        ~Device();

        Device(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(const Device&) = delete;
        Device& operator=(Device&&) = delete;

        inline const VulkanContext& GetContext() const { return *m_context; }

        BufferHandle CreateBuffer(const BufferDescription& description);
        void DestroyBuffer(Buffer* buffer);

        TextureHandle CreateTexture(const TextureDescription& description);
        void DestroyTexture(Texture* texture);

        SamplerHandle CreateSampler(const SamplerDescription& description);
        void DestroySampler(Sampler* sampler);

        ShaderHandle CreateShader(const ShaderDescription& description, const void* binary, size_t binarySize);
        void DestroyShader(Shader* shader);

        RenderTargetHandle CreateRenderTarget(const RenderTargetDescription& description);
        void DestroyRenderTarget(RenderTarget* renderTarget);

    private:
        void InitContext(const DeviceDescription& description);
        void InitMemoryContext();
        void DestroyMemoryContext();
        void DestroyContext();

    private:
        VulkanContext* m_context;
    };
}

