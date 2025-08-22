#include "RenderSystem.h"
#include "Core/Logger.h"
#include "RenderAPI/ShaderCompiler.h"
#include "Utils/FileSystem.h"
#include "Utils/GenericUtils.h"

#include "UI.h"
#include "ModelLoader.h"
#include <imgui.h>
#include "Utils/TimeUtils.h"

Mist::CIntVar CVar_ForceFrameSync("r_forceframesync", 0);

namespace rendersystem
{
    render::BindingLayoutHandle BindingLayoutCache::GetCachedLayout(const render::BindingLayoutDescription& desc)
    {
		auto it = m_cache.find(desc);
		if (it != m_cache.end())
			return it->second;
        //logfdebug("[%20s -> %3d]\n", "BindingLayoutCache", m_cache.size());
        render::BindingLayoutHandle handle = m_device->CreateBindingLayout(desc);
        m_cache[desc] = handle;
        return handle;
    }

    render::BindingSetHandle BindingCache::GetCachedBindingSet(const render::BindingSetDescription& desc)
    {
        auto it = m_cache.find(desc);
        if (it != m_cache.end())
        {
            //logfdebug("[%20s -> %3d] Reuse cached binding set\n", "BindingSetCache", m_cache.size());
            return it->second;
        }
        render::BindingLayoutDescription layoutDesc;
        for (uint32_t i = 0; i < desc.GetBindingItemCount(); ++i)
        {
            const render::BindingSetItem& item = desc.bindingItems[i];
            switch (item.type)
            {
            case render::ResourceType_TextureSRV:
                layoutDesc.PushTextureSRV(item.shaderStages, item.textures.GetSize());
                break;
            case render::ResourceType_TextureUAV:
                layoutDesc.PushTextureUAV(item.shaderStages);
                break;
            case render::ResourceType_ConstantBuffer:
                layoutDesc.PushConstantBuffer(item.shaderStages, item.bufferRange.size);
                break;
            case render::ResourceType_VolatileConstantBuffer:
                layoutDesc.PushVolatileConstantBuffer(item.shaderStages, item.bufferRange.size);
                break;
            case render::ResourceType_BufferUAV:
                layoutDesc.PushBufferUAV(item.shaderStages, item.bufferRange.size);
                break;
            default:
                unreachable_code();
                break;
            }
        }
        render::BindingLayoutHandle layout = m_layoutCache.GetCachedLayout(layoutDesc);
        render::BindingSetHandle handle = m_device->CreateBindingSet(desc, layout);
        m_cache[desc] = handle;
#if 0
        logfdebug("[%20s -> %3d]\n", "BindingSetCache", m_cache.size());
        for (uint32_t i = 0; i < desc.bindingItems.GetSize(); ++i)
        {
            const render::BindingSetItem& b = desc.bindingItems[i];
            switch (b.type)
            {
            case render::ResourceType_VolatileConstantBuffer:
            case render::ResourceType_ConstantBuffer:
                logfdebug("* Constant buffer [%2d; %2d; size: %4d; offset: %4d; buffer: 0x%p]\n", b.binding, b.shaderStages, b.bufferRange.size, b.bufferRange.offset, b.buffer.GetPtr());
                break;
            case render::ResourceType_TextureSRV:
                logfdebug("* Texture SRV     [%2d; %2d; %2d]\n", b.binding, b.shaderStages, b.textures.GetSize());
            }
        }
#endif // 0

        return handle;
    }

    void RenderSystem::Init(IWindow* window)
    {
        PROFILE_SCOPE_LOG(Init, "RenderSystem_Init");
        m_frame = 0;
        m_swapchainIndex = UINT32_MAX;
        {
            render::DeviceDescription desc;
            desc.enableValidationLayer = true;
            desc.name = "lasjd";
            desc.windowHandle = window->GetWindowHandle();
            m_device = _new render::Device(desc);
        }
        const render::Device::Swapchain& swapchain = m_device->GetSwapchain();
        uint32_t width = swapchain.width;
        uint32_t height = swapchain.height;
        m_renderResolution = { width, height };
        m_backbufferResolution = m_renderResolution;
        {
            render::TextureDescription desc;
            desc.isRenderTarget = true;
            desc.isShaderResource = true;
            desc.extent = { width, height, 1 };
            desc.format = render::Format_R8G8B8A8_UNorm;
            desc.debugName = "RT_LDR_TEXTURE";
            m_ldrTexture = m_device->CreateTexture(desc);
        }
        {
            render::TextureDescription desc;
            desc.isRenderTarget = true;
            desc.isShaderResource = true;
            desc.extent = { width, height, 1 };
            desc.format = render::Format_D24_UNorm_S8_UInt;
            desc.debugName = "RT_DEPTH_TEXTURE";
            m_depthTexture = m_device->CreateTexture(desc);
        }
        {
            render::RenderTargetDescription desc;
            desc.AddColorAttachment(m_ldrTexture);
            desc.SetDepthStencilAttachment(m_depthTexture);
            desc.debugName = "RT_LDR";
            m_ldrRt = m_device->CreateRenderTarget(desc);
        }
        {
            check(swapchain.images.size() <= FrameSyncContext::Count);
            m_presentRts.resize(swapchain.images.size());
            char buff[256];
            for (uint32_t i = 0; i < (uint32_t)swapchain.images.size(); ++i)
            {
                render::TextureHandle texture = swapchain.images[i];
                render::RenderTargetDescription desc;
                desc.AddColorAttachment(texture);
                desc.debugName = "RT_PRESENT";
                m_presentRts[i] = m_device->CreateRenderTarget(desc);
            }
            for (uint32_t i = 0; i < FrameSyncContext::Count; ++i)
            {
				sprintf_s(buff, "render semaphore %d", i);
				m_frameSyncronization.renderQueueSemaphores[i] = m_device->CreateRenderSemaphore();
				m_device->SetDebugName(m_frameSyncronization.renderQueueSemaphores[i].GetPtr(), buff);

				sprintf_s(buff, "present semaphore %d", i);
				m_frameSyncronization.presentSemaphores[i] = m_device->CreateRenderSemaphore();
				m_device->SetDebugName(m_frameSyncronization.presentSemaphores[i].GetPtr(), buff);

                m_frameSyncronization.presentSubmission[i] = 0;
            }
        }
        {
            render::TextureDescription desc;
            desc.extent = {2, 2, 1};
            desc.debugName = "default_texture";
            desc.isShaderResource = true;
            desc.format = render::Format_R8G8B8A8_UNorm;
            m_defaultTexture = m_device->CreateTexture(desc);
            render::utils::UploadContext upload(m_device);
            uint32_t data[] = { 0xffffffff, 0x00000000, 0xffffffff, 0x00000000 };
            upload.WriteTexture(m_defaultTexture, 0, 0, data, sizeof(data));
        }
        {
            m_renderContext.cmd = m_device->CreateCommandList();
        }
        {
            m_bindingCache = _new BindingCache(m_device);
            m_samplerCache = _new SamplerCache(m_device);
            m_memoryPool = _new ShaderMemoryPool(m_device);
        }

        ui::Init(m_device, m_ldrRt, window->GetWindowNative());

        InitScreenQuad();
    }

    void RenderSystem::Destroy()
    {
        m_device->WaitIdle();
        ClearState();
        m_bindingDesc.bindingItems.clear();
        m_bindingDesc.bindingItems.shrink_to_fit();
        m_memoryContextId = UINT32_MAX;
        DestroyScreenQuad();
        ui::Destroy();
        delete m_bindingCache;
        delete m_memoryPool;
        delete m_samplerCache;
        m_defaultTexture = nullptr;
        m_psoDesc = {};
        m_psoMap.clear();
        m_renderContext.cmd = nullptr;
        //for (uint32_t i = 0; i < (uint32_t)m_device->GetSwapchain().images.size(); ++i)
        for (uint32_t i = 0; i < FrameSyncContext::Count; ++i)
        {
            m_frameSyncronization.renderQueueSemaphores[i] = nullptr;
            m_frameSyncronization.presentSemaphores[i] = nullptr;
            m_frameSyncronization.frameResources[i].Clear();
        }
        m_ldrRt = nullptr;
        m_depthTexture = nullptr;
        m_ldrTexture = nullptr;
        for (uint32_t i = 0; i < (uint32_t)m_presentRts.size(); ++i)
            m_presentRts[i] = nullptr;
        delete m_device;
    }

    ShaderProgram* RenderSystem::CreateShader(const ShaderBuildDescription& desc)
    {
        ShaderProgram* shader = _new ShaderProgram(m_device, desc);
        m_shaderDb.AddProgram(shader);
        return shader;
    }

    void RenderSystem::DestroyShader(ShaderProgram** shader)
    {
        check(shader && *shader);
        m_shaderDb.RemoveProgram(*shader);
        delete *shader;
        *shader = nullptr;
    }

    void RenderSystem::ReloadAllShaders()
    {
        m_device->WaitIdle();
        m_shaderDb.ReloadAll();
        m_psoMap.clear();
    }

    render::GraphicsPipelineHandle RenderSystem::GetPso(const render::GraphicsPipelineDescription& psoDesc, render::RenderTargetHandle rt)
    {
        PROF_ZONE_SCOPED("GetPso");
        auto it = m_psoMap.find(psoDesc);
        if (it != m_psoMap.end())
            return it->second;
        render::GraphicsPipelineHandle handle = m_device->CreateGraphicsPipeline(psoDesc, rt);
        m_psoMap[psoDesc] = handle;
        //logfdebug("[%20s -> %3d]\n", "GraphicsPipelineCache", m_psoMap.size());
        return handle;
    }

    render::BindingSetHandle RenderSystem::GetBindingSet(const render::BindingSetDescription& desc)
    {
        PROF_ZONE_SCOPED("GetCachedDescriptor");
        return m_bindingCache->GetCachedBindingSet(desc);
    }

    render::SamplerHandle RenderSystem::GetSampler(const render::SamplerDescription& desc)
    {
        return m_samplerCache->GetSampler(desc);
    }

    render::SamplerHandle RenderSystem::GetSampler(render::Filter minFilter, render::Filter magFilter, render::Filter mipmapMode, render::SamplerAddressMode addressModeU, render::SamplerAddressMode addressModeV, render::SamplerAddressMode addressModeW)
    {
        render::SamplerDescription desc;
        desc.minFilter = minFilter;
        desc.magFilter = magFilter;
        desc.mipmapMode = mipmapMode;
        desc.addressModeU = addressModeU;
        desc.addressModeV = addressModeV;
        desc.addressModeW = addressModeW;
        return GetSampler(desc);
    }

    void RenderSystem::SetDefaultState()
    {
        SetDepthEnable();
        SetDepthOp();
        SetStencilEnable();
        SetStencilMask();
        SetStencilOpFront();
        SetStencilOpBack();
        SetBlendEnable();
        SetBlendFactor();
        SetBlendAlphaState();
        SetBlendWriteMask();
        SetFillMode();
        SetCullMode();
        SetViewport(0.f, 0.f,
            m_renderResolution.width,
            m_renderResolution.height);
		SetScissor(0.f, m_renderResolution.width,
            0.f, m_renderResolution.height);
        SetPrimitive();
    }

    void RenderSystem::ClearState()
    {
        m_renderContext.graphicsState = render::GraphicsState();
        m_program = nullptr;

        for (uint32_t i = 0; i < MaxTextureSlots; ++i)
        {
            for (uint32_t j = 0; j < MaxTextureBindingsPerSlot; ++j)
            {
                for (uint32_t k = 0; k < MaxTextureArrayCount; ++k)
                {
                    m_textureSlots[i][j][k] = nullptr;
                    m_samplerSlots[i][j][k] = nullptr;
                }
            }
        }
        m_renderContext.cmd->ClearState();
    }

    void RenderSystem::SetDepthEnable(bool testing, bool writing)
    {
        m_psoDesc.renderState.depthStencilState.depthTestEnable = testing;
        m_psoDesc.renderState.depthStencilState.depthWriteEnable = writing;
    }

    void RenderSystem::SetDepthOp(render::CompareOp op)
    {
        m_psoDesc.renderState.depthStencilState.depthCompareOp = op;
    }

    void RenderSystem::SetStencilEnable(bool testing)
    {
        m_psoDesc.renderState.depthStencilState.stencilTestEnable = testing;
    }

    void RenderSystem::SetStencilMask(uint8_t readMask, uint8_t writeMask, uint8_t refValue)
    {
        m_psoDesc.renderState.depthStencilState.stencilReadMask = readMask;
        m_psoDesc.renderState.depthStencilState.stencilWriteMask = writeMask;
        m_psoDesc.renderState.depthStencilState.stencilRefValue = refValue;
    }

    void RenderSystem::SetStencilOpFront(render::StencilOp fail, render::StencilOp depthFail, render::StencilOp pass, render::CompareOp compareOp)
    {
        m_psoDesc.renderState.depthStencilState.frontFace.failOp = fail;
        m_psoDesc.renderState.depthStencilState.frontFace.depthFailOp = depthFail;
        m_psoDesc.renderState.depthStencilState.frontFace.passOp = pass;
        m_psoDesc.renderState.depthStencilState.frontFace.compareOp = compareOp;
    }

    void RenderSystem::SetStencilOpBack(render::StencilOp fail, render::StencilOp depthFail, render::StencilOp pass, render::CompareOp compareOp)
    {
        m_psoDesc.renderState.depthStencilState.backFace.failOp = fail;
        m_psoDesc.renderState.depthStencilState.backFace.depthFailOp = depthFail;
        m_psoDesc.renderState.depthStencilState.backFace.passOp = pass;
        m_psoDesc.renderState.depthStencilState.backFace.compareOp = compareOp;
    }

    void RenderSystem::SetBlendEnable(bool enabled, uint32_t attachment)
    {
        GetPsoBlendStateAttachment(attachment).blendEnable = enabled;
    }

    void RenderSystem::SetBlendFactor(render::BlendFactor srcBlend, render::BlendFactor dstBlend, render::BlendOp blendOp, uint32_t attachment)
    {
        render::RenderTargetBlendState& state = GetPsoBlendStateAttachment(attachment);
        state.srcBlend = srcBlend;
        state.dstBlend = dstBlend;
        state.blendOp = blendOp;
    }

    void RenderSystem::SetBlendAlphaState(render::BlendFactor srcBlend, render::BlendFactor dstBlend, render::BlendOp blendOp, uint32_t attachment)
    {
        render::RenderTargetBlendState& state = GetPsoBlendStateAttachment(attachment);
        state.srcAlphaBlend = srcBlend;
        state.dstAlphaBlend = dstBlend;
        state.alphaBlendOp = blendOp;
    }

    void RenderSystem::SetBlendWriteMask(render::ColorMask mask, uint32_t attachment)
    {
        GetPsoBlendStateAttachment(attachment).colorWriteMask = mask;
    }

    void RenderSystem::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
    {
        m_psoDesc.renderState.viewportState.viewport = { x, y, width, height, minDepth, maxDepth };
    }

    void RenderSystem::SetViewport(const render::Viewport& viewport)
    {
        m_psoDesc.renderState.viewportState.viewport = viewport;
    }

    void RenderSystem::SetScissor(float x0, float x1, float y0, float y1)
    {
        m_psoDesc.renderState.viewportState.scissor = { x0, x1, y0, y1 };
    }

    void RenderSystem::SetScissor(const render::Rect& scissor)
    {
        m_psoDesc.renderState.viewportState.scissor = scissor;
    }

    void RenderSystem::SetFillMode(render::RasterFillMode mode)
    {
        m_psoDesc.renderState.rasterState.fillMode = mode;
    }

    void RenderSystem::SetCullMode(render::RasterCullMode mode)
    {
        m_psoDesc.renderState.rasterState.cullMode = mode;
    }

    void RenderSystem::SetPrimitive(render::PrimitiveType type)
    {
        check(type < render::PrimitiveType_MaxEnum);
        m_psoDesc.primitiveType = type;
    }

    void RenderSystem::SetShader(ShaderProgram* shader)
    {
        check(shader);
        switch (shader->m_description->type)
        {
        case ShaderProgram_Graphics:
            m_psoDesc.vertexShader = shader->m_vs;
            m_psoDesc.fragmentShader = shader->m_fs;
            m_psoDesc.vertexInputLayout = shader->m_inputLayout;
            break;
        case ShaderProgram_Compute:
            check(false);
            break;
        default:
            unreachable_code();
            break;
        }
        m_program = shader;
        m_dirtyPropertiesFlags = 0xffffffffffffffff; // all dirty

        // reserve shader properties in current shader context memory
        ShaderMemoryContext* memoryContext = m_memoryPool->GetContext(m_memoryContextId);
        check(memoryContext);
        const render::shader_compiler::ShaderReflectionProperties& properties = *m_program->m_properties;
        for (uint32_t i = 0; i < (uint32_t)properties.params.size(); ++i)
        {
            for (uint32_t j = 0; j < (uint32_t)properties.params[i].params.size(); ++j)
            {
                const render::shader_compiler::ShaderPropertyDescription& property = properties.params[i].params[j];
                switch (property.type)
                {
                case render::ResourceType_ConstantBuffer:
                case render::ResourceType_VolatileConstantBuffer:
                //case render::ResourceType_BufferUAV:
                //case render::ResourceType_DynamicBufferUAV:
                    memoryContext->ReserveProperty(property.name.c_str(), property.size);
                    break;
                }
            }
        }
    }

    void RenderSystem::SetTextureSlot(const render::TextureHandle& texture, uint32_t set, uint32_t binding, uint32_t textureIndex)
    {
        check(set < MaxTextureSlots && binding < MaxTextureBindingsPerSlot && textureIndex < MaxTextureArrayCount && texture);
        m_dirtyPropertiesFlags |= (1 << set);
        m_textureSlots[set][binding][textureIndex] = texture;
        if (render::utils::IsDepthStencilFormat(texture->m_description.format))
        {
            if (texture->GetLayoutAt(0, 0) != render::ImageLayout_DepthStencilReadOnly)
                m_renderContext.cmd->RequireTextureState({ texture, render::ImageLayout_DepthStencilReadOnly });
        }
        else if (texture->GetLayoutAt(0, 0) != render::ImageLayout_ShaderReadOnly)
            m_renderContext.cmd->RequireTextureState({ texture, render::ImageLayout_ShaderReadOnly });
    }

    void RenderSystem::SetTextureSlot(const char* id, const render::TextureHandle& texture)
    {
        check(m_program);
        uint32_t setIndex;
        const render::shader_compiler::ShaderPropertyDescription* property = m_program->GetPropertyDescription(id, &setIndex);
        check(property && setIndex != UINT32_MAX);
        check(property->arrayCount == 1);
        SetTextureSlot(texture, setIndex, property->binding);
    }

	void RenderSystem::SetTextureSlot(const render::TextureHandle* textures, uint32_t count, uint32_t set, uint32_t binding /*= 0*/)
	{
        check(count <= MaxTextureArrayCount);
        check(set < MaxTextureSlots && binding < MaxTextureBindingsPerSlot);
        for (uint32_t i = 0; i < count; ++i)
            m_textureSlots[set][binding][i] = textures[i];
        m_dirtyPropertiesFlags |= (1 << set);
	}

    void RenderSystem::SetTextureSlot(const char* id, const render::TextureHandle* textures, uint32_t count)
    {
        check(textures && count);
        if (count == 1)
        {
            SetTextureSlot(id, *textures);
        }
        else
        {
		    check(m_program);
		    uint32_t setIndex;
		    const render::shader_compiler::ShaderPropertyDescription* property = m_program->GetPropertyDescription(id, &setIndex);
		    check(property && setIndex != UINT32_MAX);
            check(count <= property->arrayCount);
            SetTextureSlot(textures, count, setIndex, property->binding);
        }
    }

	void RenderSystem::SetSampler(const render::SamplerHandle& sampler, uint32_t set, uint32_t binding, uint32_t samplerIndex)
    {
        check(samplerIndex < MaxTextureArrayCount);
        check(set < MaxTextureSlots && binding < MaxTextureBindingsPerSlot);
        m_samplerSlots[set][binding][samplerIndex] = sampler;
    }

    void RenderSystem::SetSampler(const char* id, const render::SamplerHandle& sampler)
    {
        SetSampler(id, &sampler, 1);
    }

    void RenderSystem::SetSampler(const char* id, render::Filter minFilter, render::Filter magFilter, render::Filter mipmapMode, render::SamplerAddressMode addressModeU, render::SamplerAddressMode addressModeV, render::SamplerAddressMode addressModeW, uint32_t samplerIndex)
    {
        SetSampler(id, GetSampler(minFilter, magFilter, mipmapMode, addressModeU, addressModeV, addressModeW));
    }

    void RenderSystem::SetSampler(render::Filter minFilter, render::Filter magFilter, render::Filter mipmapMode, render::SamplerAddressMode addressModeU, render::SamplerAddressMode addressModeV, render::SamplerAddressMode addressModeW, uint32_t set, uint32_t binding, uint32_t samplerIndex)
    {
        SetSampler(GetSampler(minFilter, magFilter, mipmapMode, addressModeU, addressModeV, addressModeW), set, binding, samplerIndex);
    }

	void RenderSystem::SetSampler(const char* id, const render::SamplerHandle* sampler, uint32_t count)
	{
        check(m_program);
        uint32_t setIndex;
        const render::shader_compiler::ShaderPropertyDescription* property = m_program->GetPropertyDescription(id, &setIndex);
        check(property && setIndex != UINT32_MAX);
        check(property->arrayCount >= count);
        SetSampler(sampler, count, setIndex, property->binding);
	}

	void RenderSystem::SetSampler(const render::SamplerHandle* sampler, uint32_t count, uint32_t set, uint32_t binding /*= 0*/)
	{
        for (uint32_t i = 0; i < count; ++i)
            SetSampler(sampler[i], set, binding, i);
	}

	void RenderSystem::SetShaderProperty(const char* id, const void* param, uint64_t size)
    {
        PROF_ZONE_SCOPED("SetShaderProperty");
        check(id && *id && param && size);
        ShaderMemoryContext* context = m_memoryPool->GetContext(m_memoryContextId);
        context->WriteProperty(id, param, size);
        uint32_t setIndex = UINT32_MAX;
        check(m_program->GetPropertyDescription(id, &setIndex));
        check(setIndex != UINT32_MAX);
        m_dirtyPropertiesFlags |= (1 << setIndex);
    }

    void RenderSystem::SetTextureLayout(const render::TextureHandle& texture, render::ImageLayout layout, render::TextureSubresourceRange range)
    {
        //check(!m_renderContext.cmd->IsInsideRenderPass());
        m_renderContext.cmd->RequireTextureState({ texture, layout, range });
    }

    void RenderSystem::SetTextureAsResourceBinding(render::TextureHandle texture)
    {
        if (render::utils::IsDepthFormat(texture->m_description.format))
        {
            for (uint32_t i = 0; i < texture->m_description.layers; ++i)
                SetTextureLayout(texture, render::ImageLayout_DepthStencilReadOnly, render::TextureSubresourceRange(0, 1, i, 1));
        }
        else
        {
            for (uint32_t i = 0; i < texture->m_description.layers; ++i)
                SetTextureLayout(texture, render::ImageLayout_ShaderReadOnly, render::TextureSubresourceRange(0, 1, i, 1));
        }
    }

    void RenderSystem::SetTextureAsRenderTargetAttachment(render::TextureHandle texture)
    {
        if (render::utils::IsDepthFormat(texture->m_description.format))
            SetTextureLayout(texture, render::ImageLayout_DepthAttachment);
        else if (render::utils::IsDepthFormat(texture->m_description.format))
            SetTextureLayout(texture, render::ImageLayout_DepthStencilAttachment);
        else
            SetTextureLayout(texture, render::ImageLayout_ColorAttachment);
    }

    void RenderSystem::SetRenderTarget(render::RenderTargetHandle rt)
    {
        m_renderContext.graphicsState.rt = rt;
        for (uint32_t i = 0; i < rt->m_description.colorAttachments.GetSize(); ++i)
            SetTextureAsRenderTargetAttachment(rt->m_description.colorAttachments[i].texture);
        if (rt->m_description.depthStencilAttachment.texture)
            SetTextureAsRenderTargetAttachment(rt->m_description.depthStencilAttachment.texture);
    }

    void RenderSystem::SetVertexBuffer(render::BufferHandle vb)
    {
        m_renderContext.graphicsState.vertexBuffer = vb;
    }

    void RenderSystem::SetIndexBuffer(render::BufferHandle ib)
    {
        m_renderContext.graphicsState.indexBuffer = ib;
    }

    void RenderSystem::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        FlushBeforeDraw();
        m_renderContext.cmd->Draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void RenderSystem::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t firstVertex, uint32_t firstInstance)
    {
        FlushBeforeDraw();
        m_renderContext.cmd->DrawIndexed(indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
    }

    void RenderSystem::CopyTextureToTexture(const render::TextureHandle& src, const render::TextureHandle& dst, const render::CopyTextureInfo* infoArray, uint32_t infoCount)
    {
        m_renderContext.cmd->CopyTexture(src, dst, infoArray, infoCount);
    }

    void RenderSystem::DrawFullscreenQuad()
    {
        SetVertexBuffer(m_screenQuadCopy.vb);
        SetIndexBuffer(m_screenQuadCopy.ib);
        DrawIndexed(6);
    }

    void RenderSystem::ClearColor(float r, float g, float b, float a)
    {
        m_renderContext.pendingClearColor = true;
        m_renderContext.clearColor[0] = r;
        m_renderContext.clearColor[1] = g;
        m_renderContext.clearColor[2] = b;
        m_renderContext.clearColor[3] = a;
    }

    void RenderSystem::ClearDepthStencil(float depth, uint32_t stencil)
    {
        m_renderContext.pendingClearDepthStencil = true;
        m_renderContext.clearDepth = depth;
        m_renderContext.clearStencil = stencil;
    }

    void RenderSystem::DumpState()
    {
        logerror("====== RENDER SYSTEM ======\n");
        logferror("* render frame: %lld\n", m_frame);
        logferror("* swapchain index: %d\n", m_swapchainIndex);
        if (m_swapchainIndex != UINT32_MAX)
        {
            logerror("* frame syncronization context:\n");
            logferror("\t* swapchains: %d %d %d %d %d %d\n",
                m_swapchainHistoric.Get(0),
                m_swapchainHistoric.Get(1),
                m_swapchainHistoric.Get(2),
                m_swapchainHistoric.Get(3),
                m_swapchainHistoric.Get(4),
                m_swapchainHistoric.Get(5));
        }
        logferror("* Binding cache count: %lld\n", m_bindingCache->GetCacheSize());
        logferror("* Pso cache count: %lld\n", m_psoMap.size());
        logerror("===========================\n");

    }

    render::RenderTargetBlendState& RenderSystem::GetPsoBlendStateAttachment(uint32_t attachment)
    {
        check(attachment < m_psoDesc.renderState.blendState.renderTargetBlendStates.GetCapacity());
        if (attachment >= m_psoDesc.renderState.blendState.renderTargetBlendStates.GetSize())
            m_psoDesc.renderState.blendState.renderTargetBlendStates.Resize(attachment + 1);
        return m_psoDesc.renderState.blendState.renderTargetBlendStates[attachment];
    }

    void RenderSystem::InitScreenQuad()
    {
        PROFILE_SCOPE_LOG(InitScreenQuad, "RenderSystem_InitScreenQuad");
        m_renderContext.cmd->BeginRecording();

        float quadVertices[] =
        {
            -1.f, -1.f, 0.f, 0.f, 0.f,
            -1.f, 1.f, 0.f, 0.f, 1.f,
            1.f, -1.f, 0.f, 1.f, 0.f,
            1.f, 1.f, 0.f, 1.f, 1.f,
        };
        uint32_t quadIndices[] = { 0, 1, 2, 2, 1, 3 };

        // Vertex buffer
        render::BufferDescription bufferDescription;
        bufferDescription.size = sizeof(quadVertices);
        bufferDescription.memoryUsage = render::MemoryUsage_Gpu;
        bufferDescription.bufferUsage = render::BufferUsage_VertexBuffer;
        m_screenQuadCopy.vb = m_device->CreateBuffer(bufferDescription);
        m_renderContext.cmd->WriteBuffer(m_screenQuadCopy.vb, quadVertices, sizeof(quadVertices));

        // Index buffer
        bufferDescription.size = sizeof(quadIndices);
        bufferDescription.bufferUsage = render::BufferUsage_IndexBuffer;
        m_screenQuadCopy.ib = m_device->CreateBuffer(bufferDescription);
        m_renderContext.cmd->WriteBuffer(m_screenQuadCopy.ib, quadIndices, sizeof(quadIndices));

        m_renderContext.cmd->EndRecording();
        uint64_t submissionId = m_renderContext.cmd->ExecuteCommandList();
        check(m_device->WaitForSubmissionId(submissionId));

        // Sampler
        render::SamplerDescription samplerDesc;
        samplerDesc.minFilter = render::Filter_Linear;
        samplerDesc.magFilter = render::Filter_Linear;
        m_screenQuadCopy.sampler = m_device->CreateSampler(samplerDesc);

        ShaderBuildDescription shaderDesc;
        shaderDesc.vsDesc.filePath = "shaders/quad.vert";
        shaderDesc.fsDesc.filePath = "shaders/quad.frag";
        m_screenQuadCopy.shader = _new ShaderProgram(m_device, shaderDesc);
    }

    void RenderSystem::DestroyScreenQuad()
    {
        delete m_screenQuadCopy.shader;
        m_screenQuadCopy.vb = nullptr;
        m_screenQuadCopy.ib = nullptr;
        m_screenQuadCopy.sampler = nullptr;
    }

    void RenderSystem::CopyToPresentRt(render::TextureHandle texture)
    {
        ClearState();
        SetDefaultState();
        SetRenderTarget(GetPresentRt());
        SetShader(m_screenQuadCopy.shader);
        SetTextureSlot("tex", texture);
        DrawFullscreenQuad();
        SetDefaultState();
        ClearState();
    }

    void RenderSystem::ImGuiDraw()
    {
        //ImGui::SetNextWindowPos(ImVec2(500, 500));
        ImGui::Begin("Render system");
        ImGui::SeparatorText("Draw stats");
        ImGui::Text("Tris:              %7d", m_renderContext.cmd->GetStats().tris);
        ImGui::Text("DrawCalls:         %7d", m_renderContext.cmd->GetStats().drawCalls);
        ImGui::Text("Pipelines:         %7d", m_renderContext.cmd->GetStats().pipelines);
        ImGui::Text("Render targets:    %7d", m_renderContext.cmd->GetStats().rts);
        ImGui::Text("Swapchains:        %1d %1d %1d %1d %1d %1d",
            m_swapchainHistoric.Get(0),
            m_swapchainHistoric.Get(1),
            m_swapchainHistoric.Get(2),
            m_swapchainHistoric.Get(3),
            m_swapchainHistoric.Get(4),
            m_swapchainHistoric.Get(5));

        ImGui::SeparatorText("Gpu memory");
        const render::MemoryContext& memstats = m_device->GetContext().memoryContext;
        ImGui::Text("Buffers:           %7d (%9d b/%9d b)", memstats.bufferStats.allocationCounts,
            memstats.bufferStats.currentAllocated, memstats.bufferStats.maxAllocated);
        ImGui::Text("Images:            %7d (%9d b/%9d b)", memstats.imageStats.allocationCounts,
            memstats.imageStats.currentAllocated, memstats.imageStats.maxAllocated);

        ImGui::SeparatorText("Command buffer");
        const render::CommandQueue* queue = m_device->GetCommandQueue(render::Queue_Graphics);
        ImGui::Text("CB total:          %7d", queue->GetTotalCommandBuffers());
        ImGui::Text("CB submitted:      %7d", queue->GetSubmittedCommandBuffersCount());
        ImGui::Text("CB pool:           %7d", queue->GetPoolCommandBuffersCount());

        ImGui::SeparatorText("Cache state");
        ImGui::Text("Pso cache:         %7d", m_psoMap.size());
        ImGui::Text("Pso lf:            %.4f", m_psoMap.load_factor());
        ImGui::Text("Binding cache:     %7d", m_bindingCache->GetCacheSize());
        ImGui::Text("Binding lf:        %.4f", m_bindingCache->GetLoadFactor());
        ImGui::Text("Sampler cache:     %7d", m_samplerCache->GetCacheSize());
        ImGui::Text("Sampler lf:        %.4f", m_samplerCache->GetLoadFactor());
        ImGui::Text("Shaders:           %7d", m_shaderDb.m_programs.size());
        ImGui::End();
    }

    void RenderSystem::FlushBeforeDraw()
    {
        PROF_ZONE_SCOPED("FlushBeforeDraw");
        check(m_program);
        //m_psoDesc.bindingLayouts.Clear();

        // Flush memory before process bindings
        ShaderMemoryContext* memoryContext = m_memoryPool->GetContext(m_memoryContextId);
        memoryContext->FlushMemory();

        // Bind descriptors sets and memory before draw call
        const render::shader_compiler::ShaderReflectionProperties* properties = m_program->m_properties;
        check(properties->pushConstantMap.empty());
        m_psoDesc.bindingLayouts.Resize((uint32_t)properties->params.size());
        render::BindingSetDescription& desc = m_bindingDesc;
        for (uint32_t i = 0; i < (uint32_t)properties->params.size(); ++i)
        {
            const render::shader_compiler::ShaderPropertySetDescription& paramSet = properties->params[i];
            // if is not dirty continue
            if (!(m_dirtyPropertiesFlags & (1 << paramSet.setIndex)))
                continue;

            desc.bindingItems.clear();
            desc.bindingItems.reserve(paramSet.params.size());
            for (uint32_t j = 0; j < (uint32_t)paramSet.params.size(); ++j)
            {
                const render::shader_compiler::ShaderPropertyDescription& property = paramSet.params[j];
                switch (property.type)
                {
                case render::ResourceType_TextureSRV:
                case render::ResourceType_TextureUAV:
                {
                    check(paramSet.setIndex < MaxTextureSlots && property.binding < MaxTextureBindingsPerSlot);
                    check(property.arrayCount <= MaxTextureArrayCount);
                    render::TextureHandle* tex = m_textureSlots[paramSet.setIndex][property.binding];
                    render::SamplerHandle* sampler = m_samplerSlots[paramSet.setIndex][property.binding];
                    Mist::tStaticArray<render::TextureSubresourceRange, MaxTextureArrayCount> subresources;

                    for (uint32_t k = 0; k < property.arrayCount; ++k)
                    {
#if 0
                        check(tex[k]);
#endif // 0
                        if (!tex[k])
                            tex[k] = m_defaultTexture;

                        if (!sampler[k])
                            sampler[k] = GetSampler(render::Filter_Linear, render::Filter_Linear,
                                render::Filter_Linear,
                                render::SamplerAddressMode_Repeat,
                                render::SamplerAddressMode_Repeat,
                                render::SamplerAddressMode_Repeat);
                        subresources.Push(render::TextureSubresourceRange::AllSubresources());
                    }
                    if (property.type == render::ResourceType_TextureSRV)
                        desc.PushTextureSRV(property.binding, tex, sampler, property.stage, subresources.GetData(), property.arrayCount);
                    else
                        desc.PushTextureUAV(property.binding, tex, property.stage, subresources.GetData(), property.arrayCount);
                }
                break;
                case render::ResourceType_ConstantBuffer:
                {
                    const ShaderMemoryContext::PropertyMemory* propertyMemory = memoryContext->GetProperty(property.name.c_str());
                    check(propertyMemory && propertyMemory->buffer && propertyMemory->size >= property.size);
                    desc.PushConstantBuffer(property.binding, propertyMemory->buffer.GetPtr(), property.stage, render::BufferRange(propertyMemory->offset, propertyMemory->size));
                }
                break;
                case render::ResourceType_VolatileConstantBuffer:
                    unreachable_code();
                    break;
                case render::ResourceType_BufferUAV:
                    unreachable_code();
                    break;
                case render::ResourceType_DynamicBufferUAV:
                    unreachable_code();
                    break;
                case render::ResourceType_None:
                case render::ResourceType_MaxEnum:
                default:
                    unreachable_code();
                    break;
                }
            }
            render::BindingSetHandle set = GetBindingSet(desc);
            m_renderContext.graphicsState.bindings.SetBindingSlot(paramSet.setIndex, set);
            check(paramSet.setIndex < m_psoDesc.bindingLayouts.GetSize());
            m_psoDesc.bindingLayouts[paramSet.setIndex] = set->m_layout;
            // clear dirty
            m_dirtyPropertiesFlags &= ~(1 << paramSet.setIndex);
        }

        // Fill default blend states
        if (m_psoDesc.renderState.blendState.renderTargetBlendStates.GetSize() > m_renderContext.graphicsState.rt->m_description.colorAttachments.GetSize())
        {
#if 0
            logfwarn("Render state with dirty info from previous states (BlendStates %d > Current color attachments %d)\n",
                m_psoDesc.renderState.blendState.renderTargetBlendStates.GetSize(),
                m_renderContext.graphicsState.rt->m_description.colorAttachments.GetSize());
#endif // 0

            m_psoDesc.renderState.blendState.renderTargetBlendStates.Resize(m_renderContext.graphicsState.rt->m_description.colorAttachments.GetSize());
        }
        else if (m_psoDesc.renderState.blendState.renderTargetBlendStates.GetSize() < m_renderContext.graphicsState.rt->m_description.colorAttachments.GetSize())
        {
            m_psoDesc.renderState.blendState.renderTargetBlendStates.Resize(m_renderContext.graphicsState.rt->m_description.colorAttachments.GetSize());
        }

        // Process graphics pipeline
        m_renderContext.graphicsState.pipeline = GetPso(m_psoDesc, m_renderContext.graphicsState.rt);

        m_renderContext.cmd->SetGraphicsState(m_renderContext.graphicsState);
        if (m_renderContext.pendingClearColor)
        {
            m_renderContext.pendingClearColor = false;
            m_renderContext.cmd->ClearColor(m_renderContext.clearColor[0], m_renderContext.clearColor[1], m_renderContext.clearColor[2], m_renderContext.clearColor[3]);
        }
        if (m_renderContext.pendingClearDepthStencil)
        {
            m_renderContext.pendingClearDepthStencil = false;
            m_renderContext.cmd->ClearDepthStencil(m_renderContext.clearDepth, m_renderContext.clearStencil);
        }

        FrameResourceTrack& resources = GetFrameResources();
        resources.buffers.emplace_back(m_renderContext.graphicsState.indexBuffer);
        resources.buffers.emplace_back(m_renderContext.graphicsState.vertexBuffer);
    }

    void RenderSystem::BeginFrame()
    {
        CPU_PROFILE_SCOPE(RenderSystem_BeginFrame);
        check(m_swapchainIndex == UINT32_MAX);
        m_frame++;

        ui::BeginFrame();
        ImGuiDraw();
        SetDefaultState();
        ClearState();
        m_renderContext.cmd->ResetStats();

        if (CVar_ForceFrameSync.Get())
            m_device->WaitIdle();

        const render::SemaphoreHandle& presentSemaphore = GetPresentSemaphore();

        // wait for last frame before acquire swapchain image
        if (GetPresentSubmissionId())
            m_device->WaitForSubmissionId(GetPresentSubmissionId());

        m_swapchainIndex = m_device->AcquireSwapchainIndex(presentSemaphore);
        const render::SemaphoreHandle& renderSemaphore = GetRenderSemaphore();

        render::CommandQueue* commandQueue = m_device->GetCommandQueue(render::Queue_Graphics);
        commandQueue->ProcessInFlightCommands();
        m_memoryPool->ProcessInFlight();
        commandQueue->AddWaitSemaphore(presentSemaphore, 0);
        commandQueue->AddSignalSemaphore(renderSemaphore, 0);
        GetFrameResources().Clear();

        m_memoryContextId = m_memoryPool->CreateContext();
        m_renderContext.cmd->BeginRecording();
        BeginMarker("Frame"); // frame marker
    }

    void RenderSystem::EndFrame()
    {
        CPU_PROFILE_SCOPE(RenderSystem_EndFrame);
        ui::EndFrame(m_renderContext.cmd);

        BeginMarker("CopyToPresent");
        CopyToPresentRt(m_ldrTexture);
        ClearState();
        m_renderContext.cmd->SetTextureState(render::TextureBarrier{ GetPresentRt()->m_description.colorAttachments[0].texture, render::ImageLayout_PresentSrc});
        EndMarker();

        EndMarker(); // frame marker
        m_renderContext.cmd->EndRecording();

        uint64_t submissionId = m_renderContext.cmd->ExecuteCommandList();

		const render::SemaphoreHandle& presentSemaphore = GetPresentSemaphore();
		const render::SemaphoreHandle& renderSemaphore = GetRenderSemaphore();
        {
            CPU_PROFILE_SCOPE(CpuPresent);
            m_device->Present(renderSemaphore);
        }

        m_memoryPool->Submit(submissionId, &m_memoryContextId, 1);
        SetPresentSubmissionId(submissionId);
        m_swapchainHistoric.Push(m_swapchainIndex);
        m_swapchainIndex = UINT32_MAX;
    }

    void RenderSystem::BeginMarker(const char* name, render::Color color)
    {
        m_renderContext.cmd->BeginMarker(name, color);
    }

    void RenderSystem::BeginMarkerFmt(const char* fmt, ...)
    {
		char buff[128];
		va_list lst;
		va_start(lst, fmt);
		vsprintf_s(buff, fmt, lst);
		va_end(lst);
        BeginMarker(buff);
    }

    void RenderSystem::EndMarker()
    {
        m_renderContext.cmd->EndMarker();
    }

    TextureCache::TextureCache(render::Device* device)
        : m_device(device), m_pushIndex(0)
    {
        // initial pool
        m_textures.Reserve(64);
        //for (uint32_t i = 0; i < m_textures.count; ++i)
        //    m_textures.data[i] = nullptr;
    }

    TextureCache::~TextureCache()
    {
        // integrity check. all textures must have been released before destroy cache object.
        for (uint32_t i = 0; i < m_textures.count; ++i)
            check(m_textures.data[i] == nullptr);
        m_textures.Release();
    }

    render::TextureHandle TextureCache::GetTexture(const char* name) const
    {
        if (m_map.contains(name))
        {
            uint32_t i = m_map.at(name);
            if (i != UINT32_MAX)
            {
                check(i < m_pushIndex);
                return m_textures.data[i];
            }
        }
        return nullptr;
    }

    render::TextureHandle TextureCache::GetOrCreateTexture(const render::TextureDescription& desc)
    {
        render::TextureHandle tex = GetTexture(desc.debugName.c_str());
        if (tex)
        {
            check(tex->m_description == desc);
            return tex;
        }
        check(m_pushIndex < m_textures.count);
        tex = m_device->CreateTexture(desc);
        m_textures.data[m_pushIndex] = tex;
        m_map[desc.debugName] = m_pushIndex++;
        return tex;
    }

    void TextureCache::Purge()
    {
        for (uint32_t i = m_pushIndex - 1; i < m_pushIndex; --i)
        {
            check(m_textures.data[i] && m_map.contains(m_textures.data[i]->m_description.debugName));
            check(m_map.at(m_textures.data[i]->m_description.debugName) == i);
            if (m_textures.data[i]->GetRefCounter() == 1)
            {
                // release texture
                m_map[m_textures.data[i]->m_description.debugName] = UINT32_MAX;
                m_textures.data[i] = nullptr;
                if (i != m_pushIndex - 1)
                {
                    // remap
                    m_textures.data[i] = m_textures.data[m_pushIndex - 1];
                    m_map[m_textures.data[i]->m_description.debugName] = i;
                }
                --m_pushIndex;
            }
        }
    }

    class ShaderCompiler
    {
    public:
        ShaderCompiler(render::Device* device)
            : m_device(device)
        {
            check(m_device);
        }

        ~ShaderCompiler()
        {
            ClearCachedData();
        }

        bool Compile(const ShaderFileDescription& desc, render::ShaderType stage)
        {
            check(GetShader(stage) == nullptr);
            render::shader_compiler::CompiledBinary bin = render::shader_compiler::BuildShader(desc.filePath.c_str(), stage, &desc.options);
            if (!bin.IsCompilationSucceed())
                return false;
            check(render::shader_compiler::BuildShaderParams(bin, stage, m_properties));

            render::ShaderDescription shaderDesc;
            shaderDesc.debugName = desc.filePath.c_str();
            shaderDesc.name = desc.filePath.c_str();
            shaderDesc.type = stage;
            m_shaders.push_back(m_device->CreateShader(shaderDesc, bin.binary, bin.binaryCount));

            render::shader_compiler::FreeBinary(bin);
            return true;
        }

        render::ShaderHandle GetShader(render::ShaderType stage) const
        {
            for (uint32_t i = 0; i < (uint32_t)m_shaders.size(); ++i)
            {
                check(m_shaders[i]);
                if (m_shaders[i]->m_description.type == stage)
                    return m_shaders[i];
            }
            return nullptr;
        }

        void ClearCachedData()
        {
            for (uint32_t i = 0; i < (uint32_t)m_shaders.size(); ++i)
                m_shaders[i] = nullptr;
            m_properties.params.clear();
            m_properties.pushConstantMap.clear();
        }

        void SetUniformBufferAsDynamic(const char* uniformBufferName)
        {
            check(uniformBufferName && *uniformBufferName);
            for (uint32_t i = 0; i < (uint32_t)m_properties.params.size(); ++i)
            {
                for (uint32_t j = 0; j < (uint32_t)m_properties.params[i].params.size(); ++j)
                {
                    render::shader_compiler::ShaderPropertyDescription& param = m_properties.params[i].params[j];
                    if (!strcmp(param.name.c_str(), uniformBufferName))
                    {
                        switch (param.type)
                        {
                        case render::ResourceType_ConstantBuffer: param.type = render::ResourceType_VolatileConstantBuffer; return;
                        case render::ResourceType_BufferUAV: param.type = render::ResourceType_DynamicBufferUAV; return;
                        default:
                            logferror("Trying to set as uniform dynamic an invalid Descriptor [%s]\n", uniformBufferName);
                        }
                    }
                }
            }
            check(false);
        }

        const render::shader_compiler::ShaderReflectionProperties& GetReflectionProperties() const { return m_properties; }

        render::VertexInputLayout GetVertexInputLayout() const
        {
            Mist::tStaticArray<render::VertexInputAttribute, 16> attributes;
            check(m_properties.inputLayout.attributes.size() < attributes.GetCapacity());
            attributes.Resize(Mist::limits_cast<uint32_t>(m_properties.inputLayout.attributes.size()));
            for (uint32_t i = 0; i < (uint32_t)attributes.GetSize(); ++i)
            {
                const render::shader_compiler::VertexInputAttribute& att = m_properties.inputLayout.attributes[i];
                render::Format format = render::Format_Undefined;
                check(att.size == sizeof(uint32_t)*8);
                switch (att.type)
                {
                case render::shader_compiler::AttributeType_Short:
                    if (att.count == 1)
                        format = render::Format_R16_SInt;
                    else if (att.count == 2)
                        format = render::Format_R16G16_SInt;
                    else if (att.count == 3)
                        format = render::Format_R16G16B16_SInt;
                    else if (att.count == 4)
                        format = render::Format_R16G16B16A16_SInt;
                    break;
                case render::shader_compiler::AttributeType_UShort:
					if (att.count == 1)
						format = render::Format_R16_UInt;
					else if (att.count == 2)
						format = render::Format_R16G16_UInt;
					else if (att.count == 3)
						format = render::Format_R16G16B16_UInt;
					else if (att.count == 4)
						format = render::Format_R16G16B16A16_UInt;
                    break;
                case render::shader_compiler::AttributeType_Int:
					if (att.count == 1)
						format = render::Format_R32_SInt;
					else if (att.count == 2)
						format = render::Format_R32G32_SInt;
					else if (att.count == 3)
						format = render::Format_R32G32B32_SInt;
					else if (att.count == 4)
						format = render::Format_R32G32B32A32_SInt;
                    break;
                case render::shader_compiler::AttributeType_UInt:
					if (att.count == 1)
						format = render::Format_R32_UInt;
					else if (att.count == 2)
						format = render::Format_R32G32_UInt;
					else if (att.count == 3)
						format = render::Format_R32G32B32_UInt;
					else if (att.count == 4)
						format = render::Format_R32G32B32A32_UInt;
                    break;
                case render::shader_compiler::AttributeType_Int64:
					if (att.count == 1)
						format = render::Format_R64_SInt;
					else if (att.count == 2)
						format = render::Format_R64G64_SInt;
					else if (att.count == 3)
						format = render::Format_R64G64B64_SInt;
					else if (att.count == 4)
						format = render::Format_R64G64B64A64_SInt;
                    break;
                case render::shader_compiler::AttributeType_UInt64:
					if (att.count == 1)
						format = render::Format_R64_UInt;
					else if (att.count == 2)
						format = render::Format_R64G64_UInt;
					else if (att.count == 3)
						format = render::Format_R64G64B64_UInt;
					else if (att.count == 4)
						format = render::Format_R64G64B64A64_UInt;
                    break;
                case render::shader_compiler::AttributeType_Float:
                    if (att.count == 1)
                        format = render::Format_R32_SFloat;
                    else if (att.count == 2)
                        format = render::Format_R32G32_SFloat;
                    else if (att.count == 3)
                        format = render::Format_R32G32B32_SFloat;
                    else if (att.count == 4)
                        format = render::Format_R32G32B32A32_SFloat;
                    break;
                default:
                    // the rest of the formats are not supported so far.
                    break;
                }
                check(format != render::Format_Undefined);
                attributes[att.location] = { format };
            }

            return render::VertexInputLayout::BuildVertexInputLayout(attributes.GetData(), attributes.GetSize());
        }

    private:
        render::Device* m_device;
        Mist::tDynArray<render::ShaderHandle> m_shaders;
        render::shader_compiler::ShaderReflectionProperties m_properties;
    };

    ShaderProgram::ShaderProgram(render::Device* device, const ShaderBuildDescription& description)
        : m_device(device), m_properties(nullptr)
    {
        m_description = _new ShaderBuildDescription(description);
        Reload();
    }

    ShaderProgram::~ShaderProgram()
    {
        if (m_properties)
            delete m_properties;
        m_properties = nullptr;
        delete m_description;
        m_description = nullptr;
    }

    void ShaderProgram::Reload()
    {
        switch (m_description->type)
        {
        case ShaderProgram_Graphics:
            ReloadGraphics();
            break;
        case ShaderProgram_Compute:
            ReloadCompute();
            break;
        }
        check(IsLoaded());
    }

    bool ShaderProgram::IsLoaded() const
    {
        switch (m_description->type)
        {
        case ShaderProgram_Graphics: return m_vs || m_fs;
        case ShaderProgram_Compute: return m_cs;
        }
        unreachable_code();
        return false;
    }

    void ShaderProgram::ReleaseResources()
    {
        m_vs = nullptr;
        m_fs = nullptr;
        m_cs = nullptr;
        if (m_properties)
            delete m_properties;
        m_properties = nullptr;
    }

    const render::shader_compiler::ShaderPropertyDescription* ShaderProgram::GetPropertyDescription(const char* id, uint32_t* setIndexOut) const
    {
        for (uint32_t i = 0; i < (uint32_t)m_properties->params.size(); ++i)
        {
            for (uint32_t j = 0; j < (uint32_t)m_properties->params[i].params.size(); ++j)
            {
                const render::shader_compiler::ShaderPropertyDescription* property = &m_properties->params[i].params[j];
                if (!strcmp(property->name.c_str(), id))
                {
                    if (setIndexOut)
                        *setIndexOut = m_properties->params[i].setIndex;
                    return property;
                }
            }
        }
        if (setIndexOut)
            *setIndexOut = UINT32_MAX;
        return nullptr;
    }

    bool ShaderProgram::ReloadGraphics()
    {
        check(!m_description->vsDesc.filePath.empty() || !m_description->fsDesc.filePath.empty());

        // generate shader modules
        bool succeed = false;

        ShaderCompiler compiler(m_device);
        if (!m_description->vsDesc.filePath.empty())
            succeed = compiler.Compile(m_description->vsDesc, render::ShaderType_Vertex);
        if (succeed && !m_description->fsDesc.filePath.empty())
            succeed = compiler.Compile(m_description->fsDesc, render::ShaderType_Fragment);

        if (!succeed)
        {
            logferror("Failed to generate shader modules for graphics shaders [%s, %s]\n",
                m_description->vsDesc.filePath.empty() ? "none" : m_description->vsDesc.filePath.c_str(),
                m_description->fsDesc.filePath.empty() ? "none" : m_description->fsDesc.filePath.c_str());
            return false;
        }

        if (IsLoaded())
            ReleaseResources();

        // Generate shader reflection data
        // Override with external info
        for (const ShaderDynamicBufferDescription& dynBuffer : m_description->dynamicBuffers)
            compiler.SetUniformBufferAsDynamic(dynBuffer.name.c_str());

        check(!m_properties);
        m_properties = _new render::shader_compiler::ShaderReflectionProperties();
        const render::shader_compiler::ShaderReflectionProperties& prop = compiler.GetReflectionProperties();
        m_properties->params = std::move(prop.params);
        m_properties->pushConstantMap = std::move(prop.pushConstantMap);
        m_inputLayout = compiler.GetVertexInputLayout();

        m_vs = compiler.GetShader(render::ShaderType_Vertex);
        m_fs = compiler.GetShader(render::ShaderType_Fragment);
        return true;
    }

    bool ShaderProgram::ReloadCompute()
    {
        check(!m_description->csDesc.filePath.empty());

        ShaderCompiler compiler(m_device);

        if (!compiler.Compile(m_description->csDesc, render::ShaderType_Compute))
        {
            logferror("Failed to generate shader modules for graphics shaders [%s, %s]\n",
                m_description->csDesc.filePath.empty() ? "none" : m_description->csDesc.filePath.c_str());
            return false;
        }

        if (IsLoaded())
            ReleaseResources();

        // Generate shader reflection data
        // Override with external info
        for (const ShaderDynamicBufferDescription& dynBuffer : m_description->dynamicBuffers)
            compiler.SetUniformBufferAsDynamic(dynBuffer.name.c_str());

        const render::shader_compiler::ShaderReflectionProperties& prop = compiler.GetReflectionProperties();
        m_properties->params = std::move(prop.params);
        m_properties->pushConstantMap = std::move(prop.pushConstantMap);

        m_cs = compiler.GetShader(render::ShaderType_Compute);
        return true;
    }

    ShaderProgram* ShaderProgramCache::GetOrCreateShader(render::Device* device, const ShaderBuildDescription& desc)
    {
        check(false);
        if (m_shaders.contains(desc))
            return m_shaders.at(desc);
        ShaderProgram* shader = nullptr;// _new ShaderProgram(device, desc);
        m_shaders[desc] = shader;
        return shader;
    }

    ShaderMemoryContext::ShaderMemoryContext(render::Device* device)
        : m_device(device)
    {
        check(m_device);
        check(MinTempBufferSize() == m_device->AlignUniformSize(MinTempBufferSize()));
    }

    ShaderMemoryContext::~ShaderMemoryContext()
    {
        Invalidate();
    }

    ShaderMemoryContext::ShaderMemoryContext(const ShaderMemoryContext& other)
    {
        // copy constructor must be done over empty objects.
        // just allowing use the class inside std containers.
        check(other.m_device);
        check(other.m_properties.empty());
        check(other.m_buffers.empty());
        check(other.m_freeBuffers.empty());
        check(other.m_usedBuffers.empty());

        Invalidate();
        m_device = other.m_device;
        m_pointer = other.m_pointer;
        if (other.m_size)
        {
            ResizeTempBuffer(other.m_size);
            memcpy_s(m_tempBuffer, m_size, other.m_tempBuffer, other.m_size);
        }
        m_submissionId = other.m_submissionId;
    }

    ShaderMemoryContext::ShaderMemoryContext(ShaderMemoryContext&& rvl)
    {
        check(rvl.m_device);
        Invalidate();
        m_device = rvl.m_device;
        m_buffers = std::move(rvl.m_buffers);
        m_freeBuffers = std::move(rvl.m_freeBuffers);
        m_usedBuffers = std::move(rvl.m_usedBuffers);
        m_properties = std::move(rvl.m_properties);
        m_pointer = rvl.m_pointer;
        m_tempBuffer = rvl.m_tempBuffer;
        rvl.m_tempBuffer = nullptr;
        m_size = rvl.m_size;
        m_submissionId = rvl.m_submissionId;
        rvl.Invalidate();
    }

    ShaderMemoryContext& ShaderMemoryContext::operator=(const ShaderMemoryContext& other)
    {
        new(this)ShaderMemoryContext(other);
        return *this;
    }

    ShaderMemoryContext& ShaderMemoryContext::operator=(ShaderMemoryContext&& rvl)
    {
		check(rvl.m_device);
		Invalidate();
		m_device = rvl.m_device;
		m_buffers = std::move(rvl.m_buffers);
		m_freeBuffers = std::move(rvl.m_freeBuffers);
		m_usedBuffers = std::move(rvl.m_usedBuffers);
		m_properties = std::move(rvl.m_properties);
		m_pointer = rvl.m_pointer;
		m_tempBuffer = rvl.m_tempBuffer;
		rvl.m_tempBuffer = nullptr;
		m_size = rvl.m_size;
		m_submissionId = rvl.m_submissionId;
		rvl.Invalidate();
        return *this;
    }

    void ShaderMemoryContext::ReserveProperty(const char* id, uint64_t size)
    {
        check(m_pointer == m_device->AlignUniformSize(m_pointer));
        check(size);
        size = m_device->AlignUniformSize(size);
        auto it = m_properties.find(id);
        if (it != m_properties.end())
        {
            // if already created, see if there is a buffer binded to the property
            PropertyMemory& p = it->second;
            if (p.buffer)
            {
                // if there is a buffer, override property (new property instance)
                check(p.buffer.GetRefCounter() > 1);
                p.buffer = nullptr;
                p.size = size;
                p.offset = m_pointer;
                m_pointer += size;
            }
            // if not, there is already allocated previously and it is a valid allocation
            return;
        }

        // new allocation
        PropertyMemory property;
        property.buffer = nullptr;
        property.size = size;
        property.offset = m_pointer;
        m_properties[id] = property;
        m_pointer += size;
    }

    void ShaderMemoryContext::WriteProperty(const char* id, const void* data, uint64_t size)
    {
        size = m_device->AlignUniformSize(size);
        ReserveProperty(id, size);
        const PropertyMemory* property = GetProperty(id);
        check(property);
        check(!property->buffer && property->size >= size);
        Write(data, size, 0, property->offset);
    }

    const ShaderMemoryContext::PropertyMemory* ShaderMemoryContext::GetProperty(const char* id) const
    {
        auto it = m_properties.find(id);
        if (it == m_properties.end())
            return nullptr;
        return &it->second;
    }

	void ShaderMemoryContext::BeginFrame()
	{
        uint32_t size = (uint32_t)m_usedBuffers.size();
        m_freeBuffers.resize(size);
        for (uint32_t i = 0; i < size; ++i)
            m_freeBuffers[size - i - 1] = m_usedBuffers[i];
        m_usedBuffers.clear();
	}

	void ShaderMemoryContext::FlushMemory()
    {
        if (!m_pointer)
            return;
        uint32_t bufferIndex = GetOrCreateBuffer(m_pointer);
        render::BufferHandle buffer = m_buffers[bufferIndex];
        m_usedBuffers.push_back(bufferIndex);
        m_device->WriteBuffer(buffer, m_tempBuffer, m_pointer);

        for (auto it = m_properties.begin(); it != m_properties.end(); ++it)
        {
            //check(it->second.buffer == nullptr);
            if (!it->second.buffer)
                it->second.buffer = buffer;
        }

        m_pointer = 0;
    }

    void ShaderMemoryContext::ResizeTempBuffer(uint64_t size)
    {
        size = m_device->AlignUniformSize(size);
        size = __max(size, MinTempBufferSize());
        if (m_size < size)
        {
            uint8_t* p = nullptr;
            if (m_tempBuffer)
                p = (uint8_t*)_realloc(m_tempBuffer, size);
            else
                p = (uint8_t*)_malloc(size);
            check(p);
            m_tempBuffer = p;
            m_size = size;
        }
    }

    void ShaderMemoryContext::Write(const void* data, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        if (dstOffset + size > m_size)
            ResizeTempBuffer(dstOffset + size);
        check(m_tempBuffer && dstOffset + size <= m_size);
        const uint8_t* src = (const uint8_t*)data + srcOffset;
        uint8_t* dst = m_tempBuffer + dstOffset;
        memcpy_s(dst, m_size - dstOffset, src, size);
    }

    uint32_t ShaderMemoryContext::GetOrCreateBuffer(uint64_t size)
    {
        size = m_device->AlignUniformSize(size);
        for (uint32_t i = (uint32_t)m_freeBuffers.size() - 1; i < (uint32_t)m_freeBuffers.size(); --i)
        {
            uint32_t index = m_freeBuffers[i];
            if (m_buffers[index]->m_description.size >= size)
            {
                if (i != (uint32_t)m_freeBuffers.size() - 1)
                    m_freeBuffers[i] = m_freeBuffers.back();
                m_freeBuffers.pop_back();
                return index;
            }
        }
        m_buffers.emplace_back(render::utils::CreateUniformBuffer(m_device, size, "ShaderMemoryContext_UB"));
        return (uint32_t)m_buffers.size() - 1;
    }

    void ShaderMemoryContext::Invalidate()
    {
        m_device = nullptr;
        if (m_tempBuffer)
            Mist::Free(m_tempBuffer);
        m_tempBuffer = nullptr;
        m_pointer = 0;
        m_size = 0;
        m_submissionId = UINT64_MAX;
        m_buffers.clear();
        m_properties.clear();
        m_freeBuffers.clear();
        m_usedBuffers.clear();
    }

    ShaderMemoryPool::ShaderMemoryPool(render::Device* device)
        : m_device(device)
    {
        check(m_device);
        m_contexts.reserve(10);
        m_usedContexts.reserve(10);
        m_freeContexts.reserve(10);
    }

    uint32_t ShaderMemoryPool::CreateContext()
    {
        uint64_t lastFinishedId = m_device->GetCommandQueue(render::Queue_Graphics)->GetLastSubmissionIdFinished();

        uint32_t index = UINT32_MAX;
        if (!m_freeContexts.empty())
        {
            index = m_freeContexts[m_freeContexts.size() - 1];
            check(index < (uint32_t)m_contexts.size() && m_contexts[index].m_submissionId == UINT64_MAX);
            m_freeContexts.pop_back();
        }
        else
        {
            m_contexts.emplace_back(m_device);
            //m_usedContexts.push_back((uint32_t)m_contexts.size() - 1);
            index = (uint32_t)m_contexts.size() - 1;
            m_contexts[index].m_submissionId = UINT64_MAX;
        }

        check(index < (uint32_t)m_contexts.size());
        m_contexts[index].BeginFrame();
        return index;
    }

    ShaderMemoryContext* ShaderMemoryPool::GetContext(uint32_t context)
    {
        check(context < (uint32_t)m_contexts.size());
        // integrity check: only must return used buffers with invalid submission id
        for (uint32_t i = 0; i < (uint32_t)m_usedContexts.size(); ++i)
            check(m_usedContexts[i] != context);
        check(m_contexts[context].m_submissionId == UINT64_MAX);

        return &m_contexts[context];
    }

    void ShaderMemoryPool::Submit(uint64_t submissionId, uint32_t* contexts, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            // integrity check
            uint32_t index = contexts[i];
            check(index < (uint32_t)m_contexts.size() && m_contexts[index].m_submissionId == UINT64_MAX);
            for (uint32_t j = 0; j < (uint32_t)m_usedContexts.size(); ++j)
                check(m_usedContexts[j] != index);

            m_contexts[index].m_submissionId = submissionId;
            m_usedContexts.emplace_back(index);
            contexts[i] = UINT32_MAX;
        }
    }

    void ShaderMemoryPool::ProcessInFlight()
    {
        // iterates over used contexts and store those which have finished submission id
        const uint64_t lastFinishedId = m_device->GetCommandQueue(render::Queue_Graphics)->GetLastSubmissionIdFinished();
        //logfinfo("[ShaderMemoryPool] (submissionId: %6d; used: %4d; free: %4d; created: %4d)\n",
        //    lastFinishedId, m_usedContexts.size(), m_freeContexts.size(), m_contexts.size());
        for (uint32_t i = (uint32_t)m_usedContexts.size() - 1; i < (uint32_t)m_usedContexts.size(); --i)
        {
            uint32_t index = m_usedContexts[i];
            if (m_contexts[index].m_submissionId <= lastFinishedId)
            {
                if (i != (uint32_t)m_usedContexts.size() - 1)
                    m_usedContexts[i] = m_usedContexts.back();
                m_usedContexts.pop_back();
                m_freeContexts.emplace_back(index);
                m_contexts[index].m_submissionId = UINT64_MAX;
            }
        }
		//logfinfo("[ShaderMemoryPool] (submissionId: %6d; used: %4d; free: %4d; created: %4d)\n",
		//	lastFinishedId, m_usedContexts.size(), m_freeContexts.size(), m_contexts.size());
    }

    SamplerCache::SamplerCache(render::Device* device)
        : m_device(device)
    {
        check(m_device);
    }

    SamplerCache::~SamplerCache()
    {
        m_samplers.clear();
    }

    render::SamplerHandle SamplerCache::GetSampler(const render::SamplerDescription& desc)
    {
        auto it = m_samplers.find(desc);
        if (it != m_samplers.end())
            return it->second;
        //logfdebug("[%20s -> %3d]\n", "SamplerCache", m_samplers.size());
        render::SamplerHandle sampler = m_device->CreateSampler(desc);
        m_samplers[desc] = sampler;
        return sampler;
    }
}
