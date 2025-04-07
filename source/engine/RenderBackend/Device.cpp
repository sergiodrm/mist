#include "Device.h"
#include "Utils.h"
#include "Core/Logger.h"
#include "Application/CmdParser.h"

#include "vkbootstrap/VkBootstrap.h"
#include "Application/Application.h"


namespace render
{
    Mist::CBoolVar CVar_EnableValidationLayer("r_enableValidationLayer2", true);
    Mist::CBoolVar CVar_ExitValidationLayer("r_exitValidationLayer2", true);
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
            if (!CVar_ExitValidationLayer.Get())
                Mist::Debug::PrintCallstack();
            check(!CVar_ExitValidationLayer.Get() && "Validation layer error");
        }
        return VK_FALSE;
    }

    RenderTargetDescription& RenderTargetDescription::AddColorAttachment(TextureHandle texture, const TextureSubresourceRange& range, Format format)
    {
        RenderTargetAttachment& attachment = colorAttachments.Push();
        check((format != Format_Undefined && !utils::IsDepthFormat(format) && !utils::IsStencilFormat(format)) 
            || (!utils::IsDepthFormat(texture->m_description.format) && !utils::IsStencilFormat(texture->m_description.format)));
        attachment.texture = texture;
        attachment.range = range;
        attachment.format = format;
        return *this;
    }

    RenderTargetDescription& RenderTargetDescription::SetDepthStencilAttachment(TextureHandle texture, const TextureSubresourceRange& range, Format format)
    {
        check((format != Format_Undefined && utils::IsDepthFormat(format) && utils::IsStencilFormat(format))
            || (utils::IsDepthFormat(texture->m_description.format) && utils::IsStencilFormat(texture->m_description.format)));
        depthStencilAttachment.texture = texture;
        depthStencilAttachment.range = range;
        depthStencilAttachment.format = format;
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

        if (m_views.contains(viewDescription))
            return &m_views[viewDescription];

        TextureView view;
        view.m_description = viewDescription;
        view.m_image = m_image;
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.flags = 0;
        viewInfo.pNext = nullptr;
        viewInfo.image = m_image;
        viewInfo.viewType = utils::ConvertImageViewType(viewDescription.dimension);
        viewInfo.format = utils::ConvertFormat(viewDescription.format);
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange = utils::ConvertImageSubresourceRange(description.range, description.format);
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

    RenderTarget::~RenderTarget()
    {
        check(m_device);
        m_device->DestroyRenderTarget(this);
    }

    Device::Device(const DeviceDescription& description)
        : m_context(nullptr)
    {
        InitContext(description);
        InitMemoryContext();
    }

    Device::~Device()
    {
        DestroyMemoryContext();
        DestroyContext();
    }

    BufferHandle Device::CreateBuffer(const BufferDescription& description)
    {
        Buffer* buffer = _new Buffer(this);
        buffer->m_description = description;

        VkBufferUsageFlags usage = utils::ConvertBufferUsage(description.bufferUsage);
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

        ++m_context->memoryContext.bufferStats.allocationCounts;
        m_context->memoryContext.bufferStats.currentAllocated += description.size;
        m_context->memoryContext.bufferStats.maxAllocated = __max(m_context->memoryContext.bufferStats.currentAllocated, m_context->memoryContext.bufferStats.maxAllocated);

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

    TextureHandle Device::CreateTexture(const TextureDescription& description)
    {
        check(description.extent != Extent3D(0, 0, 0) && description.format != Format_Undefined);
        Texture* texture = _new Texture(this);
        texture->m_description = description;

        VmaAllocationCreateInfo allocInfo
        {
            .usage = utils::ConvertMemoryUsage(description.memoryUsage),
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

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
        imageInfo.usage = utils::ConvertImageUsage(description.usage);
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = utils::ConvertImageLayout(description.initialLayout);
        imageInfo.pQueueFamilyIndices = nullptr;
        imageInfo.queueFamilyIndexCount = 0;

        vkcheck(vmaCreateImage(m_context->memoryContext.allocator, &imageInfo, &allocInfo,
            &texture->m_image, &texture->m_alloc, nullptr));

        ++m_context->memoryContext.imageStats.allocationCounts;
        m_context->memoryContext.imageStats.currentAllocated += texture->GetImageSize();
        m_context->memoryContext.imageStats.maxAllocated = __max(m_context->memoryContext.imageStats.currentAllocated, m_context->memoryContext.imageStats.maxAllocated);

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

        --m_context->memoryContext.imageStats.allocationCounts;
        check(m_context->memoryContext.imageStats.currentAllocated - texture->GetImageSize() < m_context->memoryContext.imageStats.currentAllocated);
        m_context->memoryContext.imageStats.currentAllocated -= texture->GetImageSize();

        vmaDestroyImage(m_context->memoryContext.allocator, texture->m_image, texture->m_alloc);
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

        vkcheck(vkCreateSampler(m_context->device, &samplerInfo, m_context->allocationCallbacks, &sampler->m_sampler));

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
        createInfo.codeSize = binarySize;
        createInfo.pCode = (const uint32_t*)binary;
        createInfo.flags = 0;
        vkcheck(vkCreateShaderModule(m_context->device, &createInfo, m_context->allocationCallbacks, &shader->m_shader));

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

            Format format = rta.format != Format_Undefined ? rta.format : texture->m_description.format;
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

            Format format = rta.format != Format_Undefined ? rta.format : texture->m_description.format;
            check(format != Format_Undefined);

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
        framebufferInfo.width = description.colorAttachments[0].texture->m_description.extent.width;
        framebufferInfo.height = description.colorAttachments[0].texture->m_description.extent.height;
        framebufferInfo.layers = 1;
        vkcheck(vkCreateFramebuffer(m_context->device, &framebufferInfo, m_context->allocationCallbacks, &renderTarget->m_framebuffer));

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
            .request_validation_layers(CVar_EnableValidationLayer.Get())
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
}
