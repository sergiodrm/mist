#pragma once

#ifndef RBE_VK
#error .
#endif

#ifdef RBE_VK
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#endif


#include "Core/Types.h"
#include "Types.h"
#include "Utils.h"

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
        Mist::String debugName;

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
        ImageDimension dimension = ImageDimension_2D;
        Extent3D extent = {0,0,1};
        MemoryUsage memoryUsage = MemoryUsage_Gpu;
        Mist::String debugName;

        bool isShaderResource = true;
        bool isRenderTarget = false;
        bool isStorageTexture = false;

        inline bool operator==(const TextureDescription& other) const
        {
            return format == other.format &&
                mipLevels == other.mipLevels &&
                layers == other.layers &&
                isShaderResource == other.isShaderResource &&
                isRenderTarget == other.isRenderTarget &&
                isStorageTexture == other.isStorageTexture &&
                dimension == other.dimension &&
                extent == other.extent &&
                memoryUsage == other.memoryUsage;
        }

        inline bool operator!=(const TextureDescription& other) const
        {
            return !(*this == other);
        }
    };

    struct TextureSubresourceLayer
    {
        uint32_t mipLevel = 0;
        uint32_t layer = 0;
        uint32_t layerCount= UINT32_MAX;

        TextureSubresourceLayer() = default;

        TextureSubresourceLayer(uint32_t _mipLevel, uint32_t _layer, uint32_t _layerCount)
            : mipLevel(_mipLevel), layer(_layer), layerCount(_layerCount) {}

        TextureSubresourceLayer Resolve(const TextureDescription& desc) const;

        inline bool operator==(const TextureSubresourceLayer& other) const
        {
            return mipLevel == other.mipLevel &&
                layer == other.layer &&
                layerCount == other.layerCount;
        }

        inline bool operator!=(const TextureSubresourceLayer& other) const { return !(*this == other); }
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
        bool viewOnlyDepth = false;
        bool viewOnlyStencil = false;

        inline bool operator==(const TextureViewDescription& other) const
        {
            return range == other.range &&
                dimension == other.dimension &&
                format == other.format &&
                viewOnlyDepth == other.viewOnlyDepth &&
                viewOnlyStencil == other.viewOnlyStencil;
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
        float minLod = FLT_MAX;
        float maxLod = FLT_MAX;
        float maxAnisotropy = 1.f;
        Mist::String debugName;

        void SetAllLODs() { minLod = FLT_MAX; maxLod = FLT_MAX; }

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
    struct hash<render::BufferRange>
    {
        size_t operator()(const render::BufferRange& range) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, range.size);
            Mist::HashCombine(seed, range.offset);
            return seed;
        }
    };

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
            Mist::HashCombine(seed, desc.isShaderResource);
            Mist::HashCombine(seed, desc.isRenderTarget);
            Mist::HashCombine(seed, desc.isStorageTexture);
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
        bool FormatSupportsLinearFiltering(Format format) const;

        inline uint32_t              GetMaxImageDimension1D() const { return physicalDeviceProperties.limits.maxImageDimension1D; }
        inline uint32_t              GetMaxImageDimension2D() const { return physicalDeviceProperties.limits.maxImageDimension2D; }
        inline uint32_t              GetMaxImageDimension3D() const { return physicalDeviceProperties.limits.maxImageDimension3D; }
        inline uint32_t              GetMaxImageDimensionCube() const { return physicalDeviceProperties.limits.maxImageDimensionCube; }
        inline uint32_t              GetMaxImageArrayLayers() const { return physicalDeviceProperties.limits.maxImageArrayLayers; }
        inline uint32_t              GetMaxTexelBufferElements() const { return physicalDeviceProperties.limits.maxTexelBufferElements; }
        inline uint32_t              GetMaxUniformBufferRange() const { return physicalDeviceProperties.limits.maxUniformBufferRange; }
        inline uint32_t              GetMaxStorageBufferRange() const { return physicalDeviceProperties.limits.maxStorageBufferRange; }
        inline uint32_t              GetMaxPushConstantsSize() const { return physicalDeviceProperties.limits.maxPushConstantsSize; }
        inline uint32_t              GetMaxMemoryAllocationCount() const { return physicalDeviceProperties.limits.maxMemoryAllocationCount; }
        inline uint32_t              GetMaxSamplerAllocationCount() const { return physicalDeviceProperties.limits.maxSamplerAllocationCount; }
        inline uint64_t              GetBufferImageGranularity() const { return physicalDeviceProperties.limits.bufferImageGranularity; }
        inline uint64_t              GetSparseAddressSpaceSize() const { return physicalDeviceProperties.limits.sparseAddressSpaceSize; }
        inline uint32_t              GetMaxBoundDescriptorSets() const { return physicalDeviceProperties.limits.maxBoundDescriptorSets; }
        inline uint32_t              GetMaxPerStageDescriptorSamplers() const { return physicalDeviceProperties.limits.maxPerStageDescriptorSamplers; }
        inline uint32_t              GetMaxPerStageDescriptorUniformBuffers() const { return physicalDeviceProperties.limits.maxPerStageDescriptorUniformBuffers; }
        inline uint32_t              GetMaxPerStageDescriptorStorageBuffers() const { return physicalDeviceProperties.limits.maxPerStageDescriptorStorageBuffers; }
        inline uint32_t              GetMaxPerStageDescriptorSampledImages() const { return physicalDeviceProperties.limits.maxPerStageDescriptorSampledImages; }
        inline uint32_t              GetMaxPerStageDescriptorStorageImages() const { return physicalDeviceProperties.limits.maxPerStageDescriptorStorageImages; }
        inline uint32_t              GetMaxPerStageDescriptorInputAttachments() const { return physicalDeviceProperties.limits.maxPerStageDescriptorInputAttachments; }
        inline uint32_t              GetMaxPerStageResources() const { return physicalDeviceProperties.limits.maxPerStageResources; }
        inline uint32_t              GetMaxDescriptorSetSamplers() const { return physicalDeviceProperties.limits.maxDescriptorSetSamplers; }
        inline uint32_t              GetMaxDescriptorSetUniformBuffers() const { return physicalDeviceProperties.limits.maxDescriptorSetUniformBuffers; }
        inline uint32_t              GetMaxDescriptorSetUniformBuffersDynamic() const { return physicalDeviceProperties.limits.maxDescriptorSetUniformBuffersDynamic; }
        inline uint32_t              GetMaxDescriptorSetStorageBuffers() const { return physicalDeviceProperties.limits.maxDescriptorSetStorageBuffers; }
        inline uint32_t              GetMaxDescriptorSetStorageBuffersDynamic() const { return physicalDeviceProperties.limits.maxDescriptorSetStorageBuffersDynamic; }
        inline uint32_t              GetMaxDescriptorSetSampledImages() const { return physicalDeviceProperties.limits.maxDescriptorSetSampledImages; }
        inline uint32_t              GetMaxDescriptorSetStorageImages() const { return physicalDeviceProperties.limits.maxDescriptorSetStorageImages; }
        inline uint32_t              GetMaxDescriptorSetInputAttachments() const { return physicalDeviceProperties.limits.maxDescriptorSetInputAttachments; }
        inline uint32_t              GetMaxVertexInputAttributes() const { return physicalDeviceProperties.limits.maxVertexInputAttributes; }
        inline uint32_t              GetMaxVertexInputBindings() const { return physicalDeviceProperties.limits.maxVertexInputBindings; }
        inline uint32_t              GetMaxVertexInputAttributeOffset() const { return physicalDeviceProperties.limits.maxVertexInputAttributeOffset; }
        inline uint32_t              GetMaxVertexInputBindingStride() const { return physicalDeviceProperties.limits.maxVertexInputBindingStride; }
        inline uint32_t              GetMaxVertexOutputComponents() const { return physicalDeviceProperties.limits.maxVertexOutputComponents; }
        inline uint32_t              GetMaxTessellationGenerationLevel() const { return physicalDeviceProperties.limits.maxTessellationGenerationLevel; }
        inline uint32_t              GetMaxTessellationPatchSize() const { return physicalDeviceProperties.limits.maxTessellationPatchSize; }
        inline uint32_t              GetMaxTessellationControlPerVertexInputComponents() const { return physicalDeviceProperties.limits.maxTessellationControlPerVertexInputComponents; }
        inline uint32_t              GetMaxTessellationControlPerVertexOutputComponents() const { return physicalDeviceProperties.limits.maxTessellationControlPerVertexOutputComponents; }
        inline uint32_t              GetMaxTessellationControlPerPatchOutputComponents() const { return physicalDeviceProperties.limits.maxTessellationControlPerPatchOutputComponents; }
        inline uint32_t              GetMaxTessellationControlTotalOutputComponents() const { return physicalDeviceProperties.limits.maxTessellationControlTotalOutputComponents; }
        inline uint32_t              GetMaxTessellationEvaluationInputComponents() const { return physicalDeviceProperties.limits.maxTessellationEvaluationInputComponents; }
        inline uint32_t              GetMaxTessellationEvaluationOutputComponents() const { return physicalDeviceProperties.limits.maxTessellationEvaluationOutputComponents; }
        inline uint32_t              GetMaxGeometryShaderInvocations() const { return physicalDeviceProperties.limits.maxGeometryShaderInvocations; }
        inline uint32_t              GetMaxGeometryInputComponents() const { return physicalDeviceProperties.limits.maxGeometryInputComponents; }
        inline uint32_t              GetMaxGeometryOutputComponents() const { return physicalDeviceProperties.limits.maxGeometryOutputComponents; }
        inline uint32_t              GetMaxGeometryOutputVertices() const { return physicalDeviceProperties.limits.maxGeometryOutputVertices; }
        inline uint32_t              GetMaxGeometryTotalOutputComponents() const { return physicalDeviceProperties.limits.maxGeometryTotalOutputComponents; }
        inline uint32_t              GetMaxFragmentInputComponents() const { return physicalDeviceProperties.limits.maxFragmentInputComponents; }
        inline uint32_t              GetMaxFragmentOutputAttachments() const { return physicalDeviceProperties.limits.maxFragmentOutputAttachments; }
        inline uint32_t              GetMaxFragmentDualSrcAttachments() const { return physicalDeviceProperties.limits.maxFragmentDualSrcAttachments; }
        inline uint32_t              GetMaxFragmentCombinedOutputResources() const { return physicalDeviceProperties.limits.maxFragmentCombinedOutputResources; }
        inline uint32_t              GetMaxComputeSharedMemorySize() const { return physicalDeviceProperties.limits.maxComputeSharedMemorySize; }
        inline void                  GetMaxComputeWorkGroupCount(uint32_t& x, uint32_t& y, uint32_t& z) const 
        {
            x = physicalDeviceProperties.limits.maxComputeWorkGroupCount[0]; 
            y = physicalDeviceProperties.limits.maxComputeWorkGroupCount[1]; 
            z = physicalDeviceProperties.limits.maxComputeWorkGroupCount[2];
        }
        inline uint32_t              GetMaxComputeWorkGroupInvocations() const { return physicalDeviceProperties.limits.maxComputeWorkGroupInvocations; }
        inline void                  GetMaxComputeWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const 
        { 
            x = physicalDeviceProperties.limits.maxComputeWorkGroupSize[0];
            y = physicalDeviceProperties.limits.maxComputeWorkGroupSize[1];
            z = physicalDeviceProperties.limits.maxComputeWorkGroupSize[2];
        }
        inline uint32_t              GetSubPixelPrecisionBits() const { return physicalDeviceProperties.limits.subPixelPrecisionBits; }
        inline uint32_t              GetSubTexelPrecisionBits() const { return physicalDeviceProperties.limits.subTexelPrecisionBits; }
        inline uint32_t              GetMipmapPrecisionBits() const { return physicalDeviceProperties.limits.mipmapPrecisionBits; }
        inline uint32_t              GetMaxDrawIndexedIndexValue() const { return physicalDeviceProperties.limits.maxDrawIndexedIndexValue; }
        inline uint32_t              GetMaxDrawIndirectCount() const { return physicalDeviceProperties.limits.maxDrawIndirectCount; }
        inline float                 GetMaxSamplerLodBias() const { return physicalDeviceProperties.limits.maxSamplerLodBias; }
        inline float                 GetMaxSamplerAnisotropy() const { return physicalDeviceProperties.limits.maxSamplerAnisotropy; }
        inline uint32_t              GetMaxViewports() const { return physicalDeviceProperties.limits.maxViewports; }
        inline void                  GetMaxViewportDimensions(uint32_t& dimX, uint32_t& dimY) const 
        { 
            dimX = physicalDeviceProperties.limits.maxViewportDimensions[0];
            dimY = physicalDeviceProperties.limits.maxViewportDimensions[1];
        }
        inline void                  GetViewportBoundsRange(float& boundMin, float& boundMax) const
        { 
            boundMin = physicalDeviceProperties.limits.viewportBoundsRange[0]; 
            boundMax = physicalDeviceProperties.limits.viewportBoundsRange[1]; 
        }
        inline uint32_t              GetViewportSubPixelBits() const { return physicalDeviceProperties.limits.viewportSubPixelBits; }
        inline size_t                GetminMemoryMapAlignment() const { return physicalDeviceProperties.limits.minMemoryMapAlignment; }
        inline uint64_t              GetMinTexelBufferOffsetAlignment() const { return physicalDeviceProperties.limits.minTexelBufferOffsetAlignment; }
        inline uint64_t              GetMinUniformBufferOffsetAlignment() const { return physicalDeviceProperties.limits.minUniformBufferOffsetAlignment; }
        inline uint64_t              GetMinStorageBufferOffsetAlignment() const { return physicalDeviceProperties.limits.minStorageBufferOffsetAlignment; }
        inline int32_t               GetMinTexelOffset() const { return physicalDeviceProperties.limits.minTexelOffset; }
        inline uint32_t              GetMaxTexelOffset() const { return physicalDeviceProperties.limits.maxTexelOffset; }
        inline int32_t               GetMinTexelGatherOffset() const { return physicalDeviceProperties.limits.minTexelGatherOffset; }
        inline uint32_t              GetMaxTexelGatherOffset() const { return physicalDeviceProperties.limits.maxTexelGatherOffset; }
        inline float                 GetMinInterpolationOffset() const { return physicalDeviceProperties.limits.minInterpolationOffset; }
        inline float                 GetMaxInterpolationOffset() const { return physicalDeviceProperties.limits.maxInterpolationOffset; }
        inline uint32_t              GetSubPixelInterpolationOffsetBits() const { return physicalDeviceProperties.limits.subPixelInterpolationOffsetBits; }
        inline uint32_t              GetMaxFramebufferWidth() const { return physicalDeviceProperties.limits.maxFramebufferWidth; }
        inline uint32_t              GetMaxFramebufferHeight() const { return physicalDeviceProperties.limits.maxFramebufferHeight; }
        inline uint32_t              GetMaxFramebufferLayers() const { return physicalDeviceProperties.limits.maxFramebufferLayers; }
        //inline VkSampleCountFlags    GetFramebufferColorSampleCounts() const {return physicalDeviceProperties.limits.framebufferColorSampleCount;s}
        //inline VkSampleCountFlags    GetFramebufferDepthSampleCounts() const {return physicalDeviceProperties.limits.framebufferDepthSampleCount;s}
        //inline VkSampleCountFlags    GetFramebufferStencilSampleCounts() const {return physicalDeviceProperties.limits.framebufferStencilSampleCount;s}
        //inline VkSampleCountFlags    GetFramebufferNoAttachmentsSampleCounts() const {return physicalDeviceProperties.limits.framebufferNoAttachmentsSampleCount;s}
        inline uint32_t              GetMaxColorAttachments() const { return physicalDeviceProperties.limits.maxColorAttachments; }
        //inline VkSampleCountFlags    GetSampledImageColorSampleCounts() const {return physicalDeviceProperties.limits.sampledImageColorSampleCount;s}
        //inline VkSampleCountFlags    GetSampledImageIntegerSampleCounts() const {return physicalDeviceProperties.limits.sampledImageIntegerSampleCount;s}
        //inline VkSampleCountFlags    GetSampledImageDepthSampleCounts() const {return physicalDeviceProperties.limits.sampledImageDepthSampleCount;s}
        //inline VkSampleCountFlags    GetSampledImageStencilSampleCounts() const {return physicalDeviceProperties.limits.sampledImageStencilSampleCount;s}
        //inline VkSampleCountFlags    GetStorageImageSampleCounts() const {return physicalDeviceProperties.limits.storageImageSampleCount;s}
        inline uint32_t              GetMaxSampleMaskWords() const { return physicalDeviceProperties.limits.maxSampleMaskWords; }
        inline uint32_t              GetTimestampComputeAndGraphics() const { return physicalDeviceProperties.limits.timestampComputeAndGraphics; }
        inline float                 GetTimestampPeriod() const { return physicalDeviceProperties.limits.timestampPeriod; }
        inline uint32_t              GetMaxClipDistances() const { return physicalDeviceProperties.limits.maxClipDistances; }
        inline uint32_t              GetMaxCullDistances() const { return physicalDeviceProperties.limits.maxCullDistances; }
        inline uint32_t              GetMaxCombinedClipAndCullDistances() const { return physicalDeviceProperties.limits.maxCombinedClipAndCullDistances; }
        inline uint32_t              GetDiscreteQueuePriorities() const { return physicalDeviceProperties.limits.discreteQueuePriorities; }
        inline void                  GetPointSizeRange(float& sizeRangeMin, float& sizeRangeMax) const 
        { 
            sizeRangeMin = physicalDeviceProperties.limits.pointSizeRange[0]; 
            sizeRangeMax = physicalDeviceProperties.limits.pointSizeRange[1]; 
        }
        inline void                  GetLineWidthRange(float& widthRangeMin, float& widthRangeMax) const 
        { 
            widthRangeMin = physicalDeviceProperties.limits.lineWidthRange[0]; 
            widthRangeMax = physicalDeviceProperties.limits.lineWidthRange[1]; 
        }
        inline float                 GetPointSizeGranularity() const { return physicalDeviceProperties.limits.pointSizeGranularity; }
        inline float                 GetLineWidthGranularity() const { return physicalDeviceProperties.limits.lineWidthGranularity; }
        inline uint32_t              GetStrictLines() const { return physicalDeviceProperties.limits.strictLines; }
        inline uint32_t              GetStandardSampleLocations() const { return physicalDeviceProperties.limits.standardSampleLocations; }
        inline uint64_t              GetPptimalBufferCopyOffsetAlignment() const { return physicalDeviceProperties.limits.optimalBufferCopyOffsetAlignment; }
        inline uint64_t              GetPptimalBufferCopyRowPitchAlignment() const { return physicalDeviceProperties.limits.optimalBufferCopyRowPitchAlignment; }
        inline uint64_t              GetNonCoherentAtomSize() const { return physicalDeviceProperties.limits.nonCoherentAtomSize; }
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
        ImageLayout GetLayoutAt(uint32_t layer = 0, uint32_t mipLevel = 0) const
        {
            TextureSubresourceRange r{mipLevel, 1, layer, 1};
            return m_layouts.at(r);
        }

        TextureDescription m_description;
        Alloc m_alloc;
        VkImage m_image;
        bool m_owner;
        Mist::tMap<TextureSubresourceRange, ImageLayout> m_layouts;
        Mist::tMap<TextureViewDescription, TextureView> m_views;
        typedef Mist::tMap<TextureSubresourceRange, ImageLayout>::const_iterator LayoutConstIterator;
        typedef Mist::tMap<TextureSubresourceRange, ImageLayout>::iterator LayoutIterator;
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
        Mist::String name;
        Mist::String entryPoint = "main";
        Mist::String debugName;
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
        
        Mist::tStaticArray<VertexInputAttribute, MaxAttributes> m_description;
        Mist::tStaticArray<VkVertexInputAttributeDescription, MaxAttributes> m_attributes;
        VkVertexInputBindingDescription m_binding;

        VertexInputLayout() : m_attributes{}, m_binding{} {}
        static VertexInputLayout BuildVertexInputLayout(const VertexInputAttribute* attributes, uint32_t count);

        inline bool operator==(const VertexInputLayout& other) const { return utils::EqualArrays(m_description.GetData(), m_description.GetSize(), other.m_description.GetData(), other.m_description.GetSize()); }
        inline bool operator!=(const VertexInputLayout& other) const { return !((*this) == other); }
    };


    /**
     * Render target
     */


    struct RenderTargetAttachment
    {
        TextureHandle texture = nullptr;
        TextureSubresourceRange range = TextureSubresourceRange(0,1,0,1);
        bool isReadOnly = false;

        inline bool IsValid() const
        {
            return texture && texture->IsAllocated() && texture->m_description.isRenderTarget;
        }
    };

    struct RenderTargetDescription
    {
        static constexpr uint32_t MaxRenderAttachments = 8;
        Mist::tStaticArray<RenderTargetAttachment, MaxRenderAttachments> colorAttachments;
        RenderTargetAttachment depthStencilAttachment;
        Mist::String debugName;

        RenderTargetDescription& AddColorAttachment(TextureHandle texture, TextureSubresourceRange range = TextureSubresourceRange{0,1,0,1});
        RenderTargetDescription& SetDepthStencilAttachment(TextureHandle texture, TextureSubresourceRange range = TextureSubresourceRange{ 0,1,0,1 });
    };

	struct RenderTargetInfo
	{
		Mist::tStaticArray<Format, RenderTargetDescription::MaxRenderAttachments> colorFormats;
		Format depthStencilFormat = Format_Undefined;
		Extent2D extent;

        RenderTargetInfo() = default;
        RenderTargetInfo(const RenderTargetDescription& description);

        inline Viewport GetViewport() const { return Viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 0); }
        inline Rect GetScissor() const { return Rect(0, static_cast<float>(extent.width), 0, static_cast<float>(extent.height)); }
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
            return utils::EqualArrays(renderTargetBlendStates.GetData(), renderTargetBlendStates.GetSize(), other.renderTargetBlendStates.GetData(), other.renderTargetBlendStates.GetSize());
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
        bool frontFaceCounterClockWise = true;
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
        uint64_t size = 0;
        uint64_t arrayCount = 0;
        ShaderType shaderType = ShaderType_None;

        BindingLayoutItem() = default;
        BindingLayoutItem(ResourceType _type, uint32_t _binding, uint64_t _size, ShaderType _shaderType, uint64_t _arrayCount)
            : type(_type),
            binding(_binding),
            size(_size),
            shaderType(_shaderType),
            arrayCount(_arrayCount)
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
        Mist::String debugName;

        BindingLayoutDescription& PushTextureSRV(ShaderType shaderType, uint64_t arrayCount) { bindings.Push(BindingLayoutItem(ResourceType_TextureSRV, bindings.GetSize(), 0, shaderType, arrayCount)); return *this; }
        BindingLayoutDescription& PushTextureUAV(ShaderType shaderType) { bindings.Push(BindingLayoutItem(ResourceType_TextureUAV, bindings.GetSize(), 0, shaderType, 1)); return *this; }
        BindingLayoutDescription& PushConstantBuffer(ShaderType shaderType, uint64_t size) { bindings.Push(BindingLayoutItem(ResourceType_ConstantBuffer, bindings.GetSize(), size, shaderType, 1)); return *this; }
        BindingLayoutDescription& PushVolatileConstantBuffer(ShaderType shaderType, uint64_t size) { bindings.Push(BindingLayoutItem(ResourceType_VolatileConstantBuffer, bindings.GetSize(), size, shaderType, 1)); return *this; }
        BindingLayoutDescription& PushBufferUAV(ShaderType shaderType, uint64_t size) { bindings.Push(BindingLayoutItem(ResourceType_BufferUAV, bindings.GetSize(), size, shaderType, 1)); return *this; }

        inline bool operator==(const BindingLayoutDescription& other) const { return utils::EqualArrays(bindings.GetData(), bindings.GetSize(), other.bindings.GetData(), other.bindings.GetSize()); }
        inline bool operator!=(const BindingLayoutDescription& other) const { return !(*this == other); }
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
        static constexpr uint32_t MaxBindingTexturesPerBinding = 8;

        // Resource can be buffer or texture or sampler
        BufferHandle buffer;
        Mist::tStaticArray<TextureHandle, MaxBindingTexturesPerBinding> textures;
        // Sampler handle used for TextureSRV
        Mist::tStaticArray<SamplerHandle, MaxBindingTexturesPerBinding> samplers;

        uint32_t binding;
        ResourceType type;
        ImageDimension dimension;
        ShaderType shaderStages;

        Mist::tStaticArray<TextureSubresourceRange, MaxBindingTexturesPerBinding> textureSubresources;
        BufferRange bufferRange;

        BindingSetItem() : buffer (nullptr) {}
        ~BindingSetItem() {}

        static BindingSetItem CreateTextureSRVItem(uint32_t slot, TextureHandle texture, SamplerHandle sampler, ShaderType shaderStages,
            TextureSubresourceRange subresource = TextureSubresourceRange::AllSubresources(), ImageDimension dimension = ImageDimension_Undefined);
		static BindingSetItem CreateTextureSRVItem(uint32_t slot, TextureHandle* textures, SamplerHandle* samplers, ShaderType shaderStages,
			TextureSubresourceRange* subresources, uint32_t count, ImageDimension dimension = ImageDimension_Undefined);
        static BindingSetItem CreateTextureUAVItem(uint32_t slot, TextureHandle texture, ShaderType shaderStages,
            TextureSubresourceRange subresource = { 0,1,0,TextureSubresourceRange::AllLayers }, ImageDimension dimension = ImageDimension_Undefined);
		static BindingSetItem CreateTextureUAVItem(uint32_t slot, TextureHandle* textures, ShaderType shaderStages,
			TextureSubresourceRange* subresources, uint32_t count, ImageDimension dimension = ImageDimension_Undefined);
        static BindingSetItem CreateConstantBufferItem(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange = BufferRange::WholeBuffer());
        static BindingSetItem CreateVolatileConstantBufferItem(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange = BufferRange::WholeBuffer());
        static BindingSetItem CreateBufferUAVItem(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange = BufferRange::WholeBuffer());

        inline bool operator==(const BindingSetItem& other) const
        {
            return buffer == other.buffer &&
                binding == other.binding &&
                type == other.type &&
                shaderStages == other.shaderStages &&
                (utils::EqualArrays(textureSubresources.GetData(), textureSubresources.GetSize(), other.textureSubresources.GetData(), other.textureSubresources.GetSize()) || bufferRange == other.bufferRange)
                && utils::EqualArrays(textures.GetData(), textures.GetSize(), other.textures.GetData(), other.textures.GetSize())
                && utils::EqualArrays(samplers.GetData(), samplers.GetSize(), other.samplers.GetData(), other.samplers.GetSize());
        }

        inline bool operator!=(const BindingSetItem& other) const
        {
            return !(*this == other);
        }
    };

    typedef Mist::tStaticArray<BindingSetItem, BindingSetItem::MaxBindingSets> BindingSetItemArray;
    typedef Mist::tDynArray<BindingSetItem> BindingSetItemDynArray;

    struct BindingSetDescription
    {
        //BindingSetItemArray bindingItems;
        BindingSetItemDynArray bindingItems;
        Mist::String debugName;

        BindingSetDescription()
        {
        }

        const BindingSetItem* GetBindingItemData() const { return bindingItems.data(); }
        uint32_t GetBindingItemCount() const { return (uint32_t)bindingItems.size(); }
        BindingSetDescription& PushItem(const BindingSetItem& item) { bindingItems.emplace_back(item); return *this; }

        BindingSetDescription& PushTextureSRV(uint32_t slot, Texture* texture, SamplerHandle sampler, ShaderType shaderStages, TextureSubresourceRange subresource = TextureSubresourceRange::AllSubresources(), ImageDimension dimension = ImageDimension_Undefined)
        {
            return PushItem(BindingSetItem::CreateTextureSRVItem(slot, texture, sampler, shaderStages, subresource, dimension));
        }

        BindingSetDescription& PushTextureSRV(uint32_t slot, TextureHandle* textures, SamplerHandle* samplers, ShaderType shaderStages, TextureSubresourceRange* subresources, uint32_t count, ImageDimension dimension = ImageDimension_Undefined)
        {
            return PushItem(BindingSetItem::CreateTextureSRVItem(slot, textures, samplers, shaderStages, subresources, count, dimension));
        }

        BindingSetDescription& PushTextureUAV(uint32_t slot, Texture* texture, ShaderType shaderStages, TextureSubresourceRange subresource = { 0,1,0,TextureSubresourceRange::AllLayers }, ImageDimension dimension = ImageDimension_Undefined)
        {
            return PushItem(BindingSetItem::CreateTextureUAVItem(slot, texture, shaderStages, subresource, dimension));
        }

        BindingSetDescription& PushTextureUAV(uint32_t slot, TextureHandle* textures, ShaderType shaderStages, TextureSubresourceRange* subresources, uint32_t count, ImageDimension dimension = ImageDimension_Undefined)
        {
            return PushItem(BindingSetItem::CreateTextureUAVItem(slot, textures, shaderStages, subresources, count, dimension));
        }

        BindingSetDescription& PushConstantBuffer(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange = BufferRange::WholeBuffer())
        {
            return PushItem(BindingSetItem::CreateConstantBufferItem(slot, buffer, shaderStages, bufferRange));
        }

        BindingSetDescription& PushVolatileConstantBuffer(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange = BufferRange::WholeBuffer())
        {
            return PushItem(BindingSetItem::CreateVolatileConstantBufferItem(slot, buffer, shaderStages, bufferRange));
        }

        BindingSetDescription& PushBufferUAV(uint32_t slot, Buffer* buffer, ShaderType shaderStages, BufferRange bufferRange = BufferRange::WholeBuffer())
        {
            return PushItem(BindingSetItem::CreateBufferUAVItem(slot, buffer, shaderStages, bufferRange));
        }

        inline bool operator==(const BindingSetDescription& other) const
        {
            return utils::EqualArrays(GetBindingItemData(), GetBindingItemCount(), other.GetBindingItemData(), other.GetBindingItemCount());
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
    typedef Mist::tStaticArray<uint32_t, BindingSet::MaxBindingSets> BindingSetDynamicOffsetsArray;

    class BindingSetVector
    {
    public:
        static constexpr uint32_t MaxDynamicsPerSet = 8;
        static constexpr uint32_t MaxSets = 8;
        struct BindingSetSlot
        {
            BindingSetHandle set = nullptr;
            Mist::tStaticArray<uint32_t, MaxDynamicsPerSet> offsets;

            inline bool operator==(const BindingSetSlot& other) const { return set == other.set && utils::EqualArrays(offsets.GetData(), offsets.GetSize(), other.offsets.GetData(), other.offsets.GetSize()); }
            inline bool operator!=(const BindingSetSlot& other) const { return !(*this == other); }
        };

        void SetBindingSlot(uint32_t slot, BindingSetHandle set, const uint32_t* offsets = nullptr, uint32_t count = 0)
        {
            check(slot < MaxSets);
            if (m_slots[slot].set != set || !utils::EqualArrays(m_slots[slot].offsets.GetData(), m_slots[slot].offsets.GetSize(), offsets, count))
            {
                m_slots[slot].set = set;
                check(count < MaxDynamicsPerSet);
                m_slots[slot].offsets.Clear();
                for (uint32_t i = 0; i < count; ++i)
                    m_slots[slot].offsets.Push(offsets[i]);
            }
        }

        void Clear()
        {
            for (uint32_t i = 0; i < MaxSets; ++i)
            {
                m_slots[i].set = nullptr;
                m_slots[i].offsets.Clear();
            }
        }

        inline bool operator==(const BindingSetVector& other) const { return utils::EqualArrays(m_slots, MaxSets, other.m_slots, MaxSets); }
        inline bool operator!=(const BindingSetVector& other) const { return !(*this == other); }

        BindingSetSlot m_slots[MaxSets];
    };

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
        Mist::String debugName;

        inline bool operator==(const GraphicsPipelineDescription& desc) const
        {
            return primitiveType == desc.primitiveType &&
                vertexShader == desc.vertexShader &&
                fragmentShader == desc.fragmentShader &&
                renderState == desc.renderState &&
                vertexInputLayout == desc.vertexInputLayout &&
                utils::EqualArrays(bindingLayouts.GetData(), bindingLayouts.GetSize(), desc.bindingLayouts.GetData(), desc.bindingLayouts.GetSize());
        }
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
        RenderTargetHandle m_rt;
		GraphicsPipelineDescription m_description;

	private:
		Device* m_device;
	};
    typedef Mist::RefPtr<GraphicsPipeline> GraphicsPipelineHandle;

    struct ComputePipelineDescription
    {
        ShaderHandle computeShader;
        BindingLayoutArray bindingLayouts;
        Mist::String debugName;
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
    /* Queries                                                              */
    /************************************************************************/

    struct QueryPoolDescription
    {
        QueryType type;
        uint32_t count;
    };

    class QueryPool : public Mist::Ref<QueryPool>
    {
    public:
        QueryPool(Device* device, const QueryPoolDescription& description)
            : m_device(device), 
            m_queryPool(VK_NULL_HANDLE), 
            m_description(description)
        { }
        ~QueryPool();

        VkQueryPool m_queryPool;
        QueryPoolDescription m_description;
    private:
        Device* m_device;
    };
    typedef Mist::RefPtr<QueryPool> QueryPoolHandle;

    /************************************************************************/
    /* Command queues and buffers                                           */
    /************************************************************************/

    uint32_t FindFamilyQueueIndex(Device* device, QueueType type);

    struct CommandBuffer
    {
        VkCommandBuffer cmd;
        VkCommandPool pool;
        QueueType type;
        uint64_t submissionId;

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
        inline uint64_t GetLastSubmissionIdFinished() const { return m_lastSubmissionIdFinished; }
        inline QueueType GetQueueType() const { return m_type; }

        inline uint32_t GetTotalCommandBuffers() const { return (uint32_t)m_createdCommandBuffers.size(); }
        inline uint32_t GetSubmittedCommandBuffersCount() const { return (uint32_t)m_submittedCommandBuffers.size(); }
        inline uint32_t GetPoolCommandBuffersCount() const { return (uint32_t)m_commandPool.size(); }

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

        BindingSetVector bindings;

        BufferHandle vertexBuffer = nullptr;
        BufferHandle indexBuffer = nullptr;

        inline bool operator ==(const GraphicsState& other) const
        {
            return pipeline == other.pipeline && 
                rt == other.rt &&
                vertexBuffer == other.vertexBuffer &&
                indexBuffer == other.indexBuffer &&
                bindings == other.bindings;
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
        ImageLayout newLayout;
        TextureSubresourceRange subresources = {0,1,0,1};
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
            uint64_t version = UINT64_MAX;
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

    struct CommandStats
    {
        uint32_t tris;
        uint32_t drawCalls;
        uint32_t rts;
        uint32_t pipelines;
    };

    struct BlitDescription
    {
        TextureHandle src;
        TextureHandle dst;
        Offset3D srcOffsets[2];
        Offset3D dstOffsets[2];
        TextureSubresourceLayer srcSubresource;
        TextureSubresourceLayer dstSubresource;
        Filter filter;
    };

    struct CopyTextureInfo
    {
        Extent3D extent;
        Offset3D srcOffset;
        TextureSubresourceLayer srcLayer;
        Offset3D dstOffset;
        TextureSubresourceLayer dstLayer;
    };

    class CommandList final : public Mist::Ref<CommandList>
    {
    public:

        CommandList(Device* device);
        ~CommandList();

        static uint64_t ExecuteCommandLists(CommandList* const* lists, uint32_t count);
        uint64_t ExecuteCommandList();

        const CommandStats& GetStats() const { return m_stats; }
        void ResetStats() { m_stats = {}; }

        void BeginRecording();
        inline bool IsRecording() const { return m_currentCommandBuffer != nullptr; }
        void EndRecording();

        void BeginMarker(const char* name, Color color = Color::White());
        void BeginMarkerFmt(const char* fmt, ...);
        void EndMarker();

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
        void RequireTextureState(const TextureBarrier& barrier);

        // Transfer
        void WriteBuffer(BufferHandle buffer, const void* data, size_t size, size_t srcOffset = 0, size_t dstOffset = 0);
        void WriteTexture(TextureHandle texture, uint32_t mipLevel, uint32_t layer, const void* data, size_t dataSize);
        void BlitTexture(const BlitDescription& desc);
        void CopyTexture(const TextureHandle& src, const TextureHandle& dst, const CopyTextureInfo* infoArray, uint32_t infoCount);

        // Queries
        void WriteQueryTimestamp(const QueryPoolHandle& query, uint32_t queryId);
        void ResetTimestampQuery(const QueryPoolHandle* queries, uint32_t count);


        inline bool AllowsCommandType(QueueType type) const { return IsRecording() && m_currentCommandBuffer->type & type; }

        void ClearState();
        CommandBuffer* GetCommandBuffer() const { check(IsRecording()); return m_currentCommandBuffer; }
        inline bool IsInsideRenderPass() const { return m_graphicsState.rt != nullptr; }
        inline Device* GetDevice() const { return m_device; }
    private:
        void BeginRenderPass(render::RenderTargetHandle rt);
        void EndRenderPass();
        void Submitted(uint64_t submissionId);
        void FlushRequiredStates();

        // Bindings
        void BindSets(const BindingSetVector& setsToBind, const BindingSetVector& currentBinding);

    private:
        Device* m_device;
        GraphicsState m_graphicsState;
        ComputeState m_computeState;
        CommandBuffer* m_currentCommandBuffer;
        bool m_dirtyBindings;

        TransferMemoryPool m_transferMemoryPool;
        Mist::tDynArray<TextureBarrier> m_requiredStates;

        CommandStats m_stats;
    };
    typedef Mist::RefPtr<CommandList> CommandListHandle;

    /**
     * Device
     */

    struct DeviceDescription
    {
        Mist::String name;
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

        uint64_t AlignUniformSize(uint64_t size) const;

        CommandListHandle CreateCommandList();
        void DestroyCommandList(CommandList* commandList);

        SemaphoreHandle CreateRenderSemaphore(bool timelineSemaphore = false);
        void DestroyRenderSemaphore(Semaphore* semaphore);
        uint64_t GetSemaphoreTimelineCounter(SemaphoreHandle semaphore);
        bool WaitSemaphore(SemaphoreHandle semaphore, uint64_t timeoutNs = 1000000000);

        BufferHandle CreateBuffer(const BufferDescription& description);
        void DestroyBuffer(Buffer* buffer);
        void WriteBuffer(BufferHandle buffer, const void* data, size_t size, size_t srcOffset = 0, size_t dstOffset = 0);
        uint64_t GetMaxPhysicalDeviceSizeInHeap(BufferUsage usage, MemoryUsage memoryUsage);

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

        QueryPoolHandle CreateQueryPool(const QueryPoolDescription& description);
        void DestroyQueryPool(QueryPool* query);
        void GetTimestampQuery(const QueryPoolHandle& query, uint32_t firstQuery, uint32_t queryCount, uint64_t* queryResults);
        void ResetQueryPool(const QueryPoolHandle& query, uint32_t firstQuery, uint32_t queryCount);

        const Swapchain& GetSwapchain() const { return m_swapchain; }
        uint32_t AcquireSwapchainIndex(SemaphoreHandle semaphoreToBeSignaled);
        void Present(SemaphoreHandle semaphoreToWait);
        void WaitIdle();

        bool WaitForSubmissionId(uint64_t submissionId, uint64_t timeoutCounter = 10) const;

        void SetDebugName(Semaphore* object, const char* debugName) const;
        void SetDebugName(Buffer* object, const char* debugName) const;
        void SetDebugName(Texture* object, const char* debugName) const;
        void SetDebugName(Sampler* object, const char* debugName) const;
        void SetDebugName(Shader* object, const char* debugName) const;
        void SetDebugName(RenderTarget* object, const char* debugName) const;
        void SetDebugName(GraphicsPipeline* object, const char* debugName) const;
        void SetDebugName(BindingLayout* object, const char* debugName) const;
        void SetDebugName(BindingSet* object, const char* debugName) const;
        void SetDebugName(ComputePipeline* object, const char* debugName) const;
        void SetDebugName(QueryPool* object, const char* debugName) const;
        void SetDebugName(const void* object, const char* debugName, uint32_t type) const;

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
        Mist::tDynArray<Buffer*> m_bufferTracking;
        Mist::tDynArray<Texture*> m_textureTracking;
    };

    namespace utils
    {
        /**
         * Buffers utilities
         */
        class UploadContext
        {
        public:
            UploadContext();
            UploadContext(Device* device);
            ~UploadContext();
            void Init(Device* device);
            void Destroy(bool waitForSubmission = true);

            void SetTextureLayout(TextureHandle texture, ImageLayout layout, uint32_t layer = 0, uint32_t mipLevel = 0);
            void WriteTexture(TextureHandle texture, uint32_t mipLevel, uint32_t layer, const void* data, size_t dataSize);
            void WriteBuffer(BufferHandle buffer, const void* data, uint64_t dataSize, uint64_t srcOffset = 0, uint64_t dstOffset = 0);
            void Blit(const BlitDescription& desc);
            uint64_t Submit(bool waitForSubmission = true);
            inline CommandListHandle GetCommandList() const { return m_cmd; }
        private:
            void BeginRecording();

        private:
            Device* m_device;
            CommandListHandle m_cmd;
        };

        BufferHandle CreateBufferAndUpload(Device* device, const void* buffer, uint64_t size, BufferUsage usage, MemoryUsage memoryUsage, UploadContext* uploadContext = nullptr, const char* debugName = nullptr);
        BufferHandle CreateVertexBuffer(Device* device, const void* buffer, uint64_t bufferSize, UploadContext* uploadContext = nullptr, const char* debugName = nullptr);
        BufferHandle CreateIndexBuffer(Device* device, const void* buffer, uint64_t bufferSize, UploadContext* uploadContext = nullptr, const char* debugName = nullptr);
        BufferHandle CreateUniformBuffer(Device* device, uint64_t size, const char* debugName = nullptr);
        BufferHandle CreateDynamicVertexBuffer(Device* device, uint64_t bufferSize, const char* debugName);

        /**
         * Shader utilities
         */
        ShaderHandle BuildShader(Device* device, const char* filepath, ShaderType type);
        ShaderHandle BuildVertexShader(Device* device, const char* filepath);
        ShaderHandle BuildFragmentShader(Device* device, const char* filepath);
    }
}


// hashes
namespace std
{
    template <>
    struct hash<render::StencilOpState>
    {
        size_t operator()(const render::StencilOpState& state) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, state.compareOp);
            Mist::HashCombine(seed, state.passOp);
            Mist::HashCombine(seed, state.depthFailOp);
            Mist::HashCombine(seed, state.failOp);
            return seed;
        }
    };

    template <>
    struct hash<render::DepthStencilState>
    {
        size_t operator()(const render::DepthStencilState& state) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, state.depthTestEnable);
            Mist::HashCombine(seed, state.depthWriteEnable);
            Mist::HashCombine(seed, state.depthCompareOp);
            Mist::HashCombine(seed, state.stencilTestEnable);
            Mist::HashCombine(seed, state.stencilReadMask);
            Mist::HashCombine(seed, state.stencilWriteMask);
            Mist::HashCombine(seed, state.stencilRefValue);
            Mist::HashCombine(seed, state.frontFace);
            Mist::HashCombine(seed, state.backFace);
            return seed;
        }
    };

    template <>
    struct hash<render::RasterState>
    {
        size_t operator()(const render::RasterState& state) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, state.cullMode);
            Mist::HashCombine(seed, state.depthBiasClamp);
            Mist::HashCombine(seed, state.depthBiasConstantFactor);
            Mist::HashCombine(seed, state.depthBiasEnable);
            Mist::HashCombine(seed, state.depthBiasSlopeFactor);
            Mist::HashCombine(seed, state.depthClampEnable);
            Mist::HashCombine(seed, state.fillMode);
            Mist::HashCombine(seed, state.cullMode);
            Mist::HashCombine(seed, state.frontFaceCounterClockWise);
            Mist::HashCombine(seed, state.scissorEnable);
            return seed;
        }
    };
    
    template <>
    struct hash<render::RenderTargetBlendState>
    {
        size_t operator()(const render::RenderTargetBlendState& state) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, state.blendEnable);
            Mist::HashCombine(seed, state.srcBlend);
            Mist::HashCombine(seed, state.dstBlend);
            Mist::HashCombine(seed, state.blendOp);
            Mist::HashCombine(seed, state.srcAlphaBlend);
            Mist::HashCombine(seed, state.dstAlphaBlend);
            Mist::HashCombine(seed, state.alphaBlendOp);
            Mist::HashCombine(seed, state.colorWriteMask);
            return seed;
        }
    };

    template <>
    struct hash<render::BlendState>
    {
        size_t operator()(const render::BlendState& state) const
        {
            size_t seed = 0;
            for (uint32_t i = 0; i < state.renderTargetBlendStates.GetSize(); ++i)
                Mist::HashCombine(seed, state.renderTargetBlendStates[i]);
            return seed;
        }
    };

    template <>
    struct hash<render::ViewportState>
    {
        size_t operator()(const render::ViewportState& state) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, state.viewport.x);
            Mist::HashCombine(seed, state.viewport.y);
            Mist::HashCombine(seed, state.viewport.width);
            Mist::HashCombine(seed, state.viewport.height);
            Mist::HashCombine(seed, state.viewport.minDepth);
            Mist::HashCombine(seed, state.viewport.maxDepth);
            return seed;
        }
    };

    template <>
    struct hash<render::RenderState>
    {
        size_t operator()(const render::RenderState& state) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, state.viewportState);
            Mist::HashCombine(seed, state.rasterState);
            Mist::HashCombine(seed, state.blendState);
            Mist::HashCombine(seed, state.depthStencilState);
            return seed;
        }
    };

    template <>
    struct hash<render::VertexInputLayout>
    {
        size_t operator()(const render::VertexInputLayout& layout) const
        {
            size_t seed = 0;
            for (uint32_t i = 0; i < layout.m_attributes.GetSize(); ++i)
            {
                Mist::HashCombine(seed, layout.m_attributes[i].location);
                Mist::HashCombine(seed, layout.m_attributes[i].format);
            }
            return seed;
        }
    };

    template <>
    struct hash<render::ShaderDescription>
    {
        size_t operator()(const render::ShaderDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.type);
            Mist::HashCombine(seed, desc.name);
            return seed;
        }
    };

    template <>
    struct hash<render::GraphicsPipelineDescription>
    {
        size_t operator()(const render::GraphicsPipelineDescription& desc) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, desc.primitiveType);
            Mist::HashCombine(seed, desc.renderState);
            Mist::HashCombine(seed, desc.vertexInputLayout);
            if (desc.vertexShader)
                Mist::HashCombine(seed, desc.vertexShader->m_description);
            if (desc.fragmentShader)
                Mist::HashCombine(seed, desc.fragmentShader->m_description);
            return seed;
        }
    };

    template<>
    struct hash<render::BindingSetItem>
    {
        size_t operator()(const render::BindingSetItem& item) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, item.binding);
            Mist::HashCombine(seed, item.type);
            switch (item.type)
            {
            case render::ResourceType_TextureSRV:
            case render::ResourceType_TextureUAV:
                Mist::HashCombine(seed, item.dimension);
                for (uint32_t i = 0; i < item.textures.GetSize(); ++i)
                {
                    Mist::HashCombine(seed, item.samplers[i].GetPtr());
                    Mist::HashCombine(seed, item.textures[i].GetPtr());
                    Mist::HashCombine(seed, item.textureSubresources[i]);
                }
                break;
            case render::ResourceType_ConstantBuffer:
            case render::ResourceType_VolatileConstantBuffer:
            case render::ResourceType_BufferUAV:
                Mist::HashCombine(seed, item.bufferRange);
                Mist::HashCombine(seed, item.buffer.GetPtr());
                break;
            default:
                unreachable_code();
                break;
            }
            return seed;
        }
    };

    template <>
    struct hash<render::BindingSetDescription>
    {
        size_t operator()(const render::BindingSetDescription& desc) const
        {
            size_t seed = 0;
            for (uint32_t i = 0; i < desc.GetBindingItemCount(); ++i)
                Mist::HashCombine(seed, desc.bindingItems[i]);
            return seed;
        }
    };

    template<>
    struct hash<render::BindingLayoutItem>
    {
        size_t operator()(const render::BindingLayoutItem& item) const
        {
            size_t seed = 0;
            Mist::HashCombine(seed, item.binding);
            Mist::HashCombine(seed, item.type);
            Mist::HashCombine(seed, item.shaderType);
            switch (item.type)
            {
            case render::ResourceType_ConstantBuffer:
            case render::ResourceType_VolatileConstantBuffer:
            case render::ResourceType_BufferUAV:
                Mist::HashCombine(seed, item.size);
                break;
            case render::ResourceType_TextureSRV:
            case render::ResourceType_TextureUAV:
                Mist::HashCombine(seed, item.arrayCount);
                break;
            }
            return seed;
        }
    };

    template <>
    struct hash<render::BindingLayoutDescription>
    {
        size_t operator()(const render::BindingLayoutDescription& desc) const
        {
            size_t seed = 0;
            for (uint32_t i = 0; i < desc.bindings.GetSize(); ++i)
                Mist::HashCombine(seed, desc.bindings[i]);
            return seed;
        }
    };
}
