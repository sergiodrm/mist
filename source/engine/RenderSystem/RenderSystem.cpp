#include "RenderSystem.h"
#include "Core/Logger.h"
#include "RenderAPI/ShaderCompiler.h"
#include "Utils/FileSystem.h"
#include "Utils/GenericUtils.h"

#include "UI.h"
#include <imgui.h>

namespace rendersystem
{
    render::BindingLayoutHandle BindingLayoutCache::GetCachedLayout(const render::BindingLayoutDescription& desc)
    {
        if (m_cache.contains(desc))
            return m_cache.at(desc);
        m_cache[desc] = m_device->CreateBindingLayout(desc);
        return m_cache.at(desc);
    }

    render::BindingSetHandle BindingCache::GetCachedBindingSet(const render::BindingSetDescription& desc)
    {
        if (m_cache.contains(desc))
            return m_cache.at(desc);
        render::BindingLayoutDescription layoutDesc;
        for (uint32_t i = 0; i < desc.bindingItems.GetSize(); ++i)
        {
            const render::BindingSetItem& item = desc.bindingItems[i];
            switch (item.type)
            {
            case render::ResourceType_TextureSRV:
                layoutDesc.PushTextureSRV(item.shaderStages);
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
        m_cache[desc] = m_device->CreateBindingSet(desc, layout);
        return m_cache.at(desc);
    }

    ShaderMemoryPool::ShaderMemoryPool(render::Device* device, uint64_t minChunkSize)
        : m_device(device), m_currentChunk(UINT32_MAX)
    {
        check(device);
        m_minChunkSize = m_device->AlignUniformSize(minChunkSize);
        static constexpr uint64_t initialSize = 5;
        m_pool.reserve(initialSize);
        m_submitted.reserve(initialSize);
        m_toSubmit.reserve(initialSize);
    }

    ShaderMemoryPool::~ShaderMemoryPool()
    {
        m_pool.clear();
        m_submitted.clear();
        m_toSubmit.clear();
        m_allocations.clear();
    }

    ShaderMemoryPool::MemoryAllocation ShaderMemoryPool::Allocate(const char* id, uint64_t size)
    {
        size = m_device->AlignUniformSize(size);

        // check if already allocated. This must be on the same frame before submit, so in allocation maps should not be stored chunk reference with valid submission id (buffer in flight).
        auto it = m_allocations.find(id);
        if (it != m_allocations.end())
        {
            MemoryAllocation& allocation = it->second;
            // if allocation is valid, there is an alloc not submitted yet.
            if (allocation.IsValid())
            {
                Chunk& chunk = m_pool[allocation.chunkIndex];
                check(chunk.submissionId == UINT64_MAX && chunk.buffer->m_description.size >= size);
                return allocation;
            }
        }

        // No created or allocation caducated.
        if (m_currentChunk != UINT32_MAX)
        {
            Chunk& chunk = m_pool[m_currentChunk];
            check(chunk.submissionId == UINT64_MAX);
            if (chunk.pointer + size <= chunk.buffer->m_description.size)
            {
                // current chunk has space enough
                MemoryAllocation allocation;
                allocation.chunkIndex = m_currentChunk;
                allocation.pointer = chunk.pointer;
                allocation.size = size;
                chunk.pointer += size;
                m_allocations[id] = allocation;
                return allocation;
            }

            m_toSubmit.push_back(m_currentChunk);
            m_currentChunk = UINT32_MAX;
        }
        check(m_currentChunk == UINT32_MAX);
        m_currentChunk = GetFreeChunk(size);
        Chunk& chunk = m_pool[m_currentChunk];
        check(chunk.buffer && chunk.pointer == 0 && chunk.submissionId == UINT64_MAX);

        MemoryAllocation allocation;
        allocation.chunkIndex = m_currentChunk;
        allocation.pointer = chunk.pointer;
        allocation.size = size;
        m_allocations[id] = allocation;
        chunk.pointer += size;
        return allocation;
    }

    ShaderMemoryPool::MemoryAllocation ShaderMemoryPool::Find(const char* id) const
    {
        return m_allocations.contains(id) ? m_allocations.at(id) : MemoryAllocation::InvalidAllocation();
    }

    void ShaderMemoryPool::Write(const MemoryAllocation& allocation, const void* src, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        check(allocation.IsValid() && src && size);
        Chunk& chunk = m_pool[allocation.chunkIndex];
        check(chunk.submissionId == UINT64_MAX);
        m_device->WriteBuffer(chunk.buffer, src, size, srcOffset, dstOffset + allocation.pointer);
    }

    void ShaderMemoryPool::Submit(uint64_t submissionId)
    {
        // Submit id into used buffers
        for (uint32_t i = 0; i < (uint32_t)m_toSubmit.size(); ++i)
        {
            Chunk& chunk = m_pool[m_toSubmit[i]];
            check(chunk.submissionId == UINT64_MAX);
            m_submitted.push_back(m_toSubmit[i]);
            chunk.submissionId = submissionId;
        }
        m_toSubmit.clear();
        // Reset allocation map
        for (auto it = m_allocations.begin(); it != m_allocations.end(); ++it)
        {
            it->second = MemoryAllocation::InvalidAllocation();
        }
    }

    uint32_t ShaderMemoryPool::GetFreeChunk(uint64_t size)
    {
        // search into submitted if there is one already finished
        uint64_t lastFinishedId = m_device->GetCommandQueue(render::Queue_Graphics)->GetLastSubmissionIdFinished();
        for (uint32_t i = (uint32_t)m_submitted.size() - 1; i < (uint32_t)m_submitted.size(); --i)
        {
            uint32_t chunkIndex = m_submitted[i];
            Chunk& chunk = m_pool[chunkIndex];
            check(chunk.submissionId != UINT64_MAX);
            if (chunk.submissionId <= lastFinishedId && chunk.buffer->m_description.size >= size)
            {
                chunk.pointer = 0;
                chunk.submissionId = UINT64_MAX;
                // take last value in vector and replace current index.
                if (i == (uint32_t)m_submitted.size() - 1)
                    m_submitted.pop_back();
                else
                {
                    m_submitted[i] = m_submitted.back();
                    m_submitted.pop_back();
                }
                return chunkIndex;
            }
        }
        return CreateChunk(size);
    }

    uint32_t ShaderMemoryPool::CreateChunk(uint64_t size)
    {
        size = m_device->AlignUniformSize(size);
        render::BufferDescription desc;
        desc.size = size;
        desc.bufferUsage = render::BufferUsage_UniformBuffer;
        desc.memoryUsage = render::MemoryUsage_CpuToGpu;
        Chunk chunk;
        chunk.buffer = m_device->CreateBuffer(desc);
        chunk.pointer = 0;
        chunk.submissionId = UINT64_MAX;
        m_pool.push_back(chunk);
        return (uint32_t)m_pool.size() - 1;
    }

    void RenderSystem::Init(IWindow* window)
    {
        m_frame = 0;
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
        {
            render::TextureDescription desc;
            desc.isRenderTarget = true;
            desc.isShaderResource = true;
            desc.extent = {width, height, 1};
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
            m_presentRts.resize(swapchain.images.size());
            for (uint32_t i = 0; i < (uint32_t)swapchain.images.size(); ++i)
            {
                render::TextureHandle texture = swapchain.images[i];
                render::RenderTargetDescription desc;
                desc.AddColorAttachment(texture);
                desc.debugName = "RT_PRESENT";
                m_presentRts[i] = m_device->CreateRenderTarget(desc);
            }
        }
        {
            check(swapchain.images.size() <= Mist::CountOf(m_frameSyncronization));
            for (uint32_t i = 0; i < (uint32_t)swapchain.images.size(); ++i)
            {
                char buff[256];
                sprintf_s(buff, "render semaphore %d", i);
                m_frameSyncronization[i].renderQueueSemaphore = m_device->CreateRenderSemaphore();
                m_device->SetDebugName(m_frameSyncronization[i].renderQueueSemaphore.GetPtr(), buff);
                
                sprintf_s(buff, "present semaphore %d", i);
                m_frameSyncronization[i].presentSemaphore = m_device->CreateRenderSemaphore();
                m_device->SetDebugName(m_frameSyncronization[i].presentSemaphore.GetPtr(), buff);
            }
        }
        {
            m_cmd = m_device->CreateCommandList();
        }
        {
            m_memoryPool = _new ShaderMemoryPool(m_device);
            m_bindingCache = _new BindingCache(m_device);
        }

        ui::Init(m_device, m_ldrRt, window->GetWindowNative());

        InitScreenQuad();

        // init quad model
        m_cmd->BeginRecording();

        float quadVertices[] =
        {
            -1.f, -1.f, -1.f, 0.f, 0.f,
            -1.f, 1.f, -1.f, 0.f, 1.f,
            1.f, -1.f, -1.f, 1.f, 0.f,
            1.f, 1.f, -1.f, 1.f, 1.f,
        };
        uint32_t quadIndices[] = { 0, 1, 2, 2, 1, 3 };

        m_drawList.models.Reserve(1);
        Model& model = m_drawList.models.data[0];
        model.meshes.Reserve(1);
        Mesh& mesh = model.meshes.data[0];

        // Vertex buffer
        render::BufferDescription bufferDescription;
        bufferDescription.size = sizeof(quadVertices);
        bufferDescription.memoryUsage = render::MemoryUsage_Gpu;
        bufferDescription.bufferUsage = render::BufferUsage_VertexBuffer;
        mesh.vb = m_device->CreateBuffer(bufferDescription);
        m_cmd->WriteBuffer(mesh.vb, quadVertices, sizeof(quadVertices));

        // Index buffer
        bufferDescription.size = sizeof(quadIndices);
        bufferDescription.bufferUsage = render::BufferUsage_IndexBuffer;
        mesh.ib = m_device->CreateBuffer(bufferDescription);
        m_cmd->WriteBuffer(mesh.ib, quadIndices, sizeof(quadIndices));

        render::VertexInputAttribute attributes[2] = { render::Format_R32G32B32_SFloat, render::Format_R32G32_SFloat };
        mesh.inputLayout = render::VertexInputLayout::BuildVertexInputLayout(attributes, Mist::CountOf(attributes));
        
        mesh.materials.Reserve(1);

        Material& material = mesh.materials.data[0];
        material.color[0] = 1.f;
        material.color[1] = 0.f;
        material.color[2] = 0.f;
        material.color[3] = 1.f;
        // Shader modules
        render::shader_compiler::CompiledBinary vsSrc = render::shader_compiler::Compile(Mist::cAssetPath("shaders/world_quad.vert"), render::ShaderType_Vertex);
        render::shader_compiler::CompiledBinary fsSrc = render::shader_compiler::Compile(Mist::cAssetPath("shaders/world_quad.frag"), render::ShaderType_Fragment);

        material.vs = m_device->CreateShader({ .type = render::ShaderType_Vertex }, vsSrc.binary, vsSrc.binaryCount);
        material.fs = m_device->CreateShader({ .type = render::ShaderType_Fragment }, fsSrc.binary, fsSrc.binaryCount);

        mesh.primitives.Reserve(1);
        Primitive& primitive = mesh.primitives.data[0];
        primitive.count = 6;
        primitive.first = 0;
        primitive.mesh = 0;
        primitive.material = 0;

        m_cmd->EndRecording();
        uint64_t id = m_cmd->ExecuteCommandList();
        check(m_device->WaitForSubmissionId(id));

        // create instances
        static constexpr uint32_t w = 10;
        static constexpr uint32_t h = 10;
        m_drawList.modelInstances.Reserve(w*h);
        for (uint32_t i = 0; i < w; ++i)
        {
            for (uint32_t j = 0; j < h; ++j)
            {
                uint32_t modelInstanceIndex = j * h + i;
                ModelInstance& m = m_drawList.modelInstances.data[modelInstanceIndex];
                glm::vec4 pos((float)i * 5.f, 0.f, (float)j * 5.f, 1.f);
                m.transform = Mist::math::PosToMat4(pos);
                m.model = 0;
            }
        }
    }

    void RenderSystem::Destroy()
    {
        m_device->WaitIdle();
        m_drawList.Release();
        DestroyScreenQuad();
        ui::Destroy();
        delete m_bindingCache;
        delete m_memoryPool;
        m_psoMap.clear();
        m_cmd = nullptr;
        for (uint32_t i = 0; i < (uint32_t)m_device->GetSwapchain().images.size(); ++i)
        {
            m_frameSyncronization[i].renderQueueSemaphore = nullptr;
            m_frameSyncronization[i].presentSemaphore = nullptr;
        }
        m_ldrRt = nullptr;
        m_depthTexture = nullptr;
        m_ldrTexture = nullptr;
        for (uint32_t i = 0; i < (uint32_t)m_presentRts.size(); ++i)
            m_presentRts[i] = nullptr;
        delete m_device;
    }

    void RenderSystem::Draw()
    {
        //BeginFrame();
        render::GraphicsState state;
        state.rt = m_ldrRt;
        m_cmd->SetGraphicsState(state);
        m_cmd->ClearColor(0.2f, 0.2f, 0.2f, 1.f);
        DrawDrawList(m_drawList);
        //EndFrame();
    }

    void RenderSystem::SetViewProjection(const glm::mat4& view, const glm::mat4& projection)
    {
        m_drawList.view = view;
        m_drawList.projection = projection;
    }

    render::GraphicsPipelineHandle RenderSystem::GetPso(const render::GraphicsPipelineDescription& psoDesc, render::RenderTargetHandle rt)
    {
        if (m_psoMap.contains(psoDesc))
            return m_psoMap[psoDesc];
        m_psoMap[psoDesc] = m_device->CreateGraphicsPipeline(psoDesc, rt);
        return m_psoMap.at(psoDesc);
    }

    void RenderSystem::DrawDrawList(const DrawList& list)
    {
        render::RenderTargetHandle rt = m_ldrRt;
        render::GraphicsPipelineDescription psoDesc;
        psoDesc.renderState.viewportState.viewport = rt->m_info.GetViewport();
        psoDesc.renderState.viewportState.scissor = rt->m_info.GetScissor();
        psoDesc.renderState.blendState.renderTargetBlendStates.Push(render::RenderTargetBlendState());

        render::BindingSetHandle viewProjectionSet;
        render::BindingSetHandle modelTransformSet;

        struct  
        {
            glm::mat4 view;
            glm::mat4 projection;
            glm::mat4 viewProjection;
            glm::mat4 invView;
            glm::mat4 invViewProjection;
        } viewProjectionData;
        viewProjectionData.view = list.view;
        viewProjectionData.projection = list.projection;
        viewProjectionData.viewProjection = list.projection * list.view;
        viewProjectionData.invView = glm::inverse(list.view);
        viewProjectionData.invViewProjection = list.projection * viewProjectionData.invView;

        ShaderMemoryPool::MemoryAllocation viewProjAllocation = m_memoryPool->Allocate("viewProjection", sizeof(viewProjectionData));
        m_memoryPool->Write(viewProjAllocation, &viewProjectionData, sizeof(viewProjectionData));
        render::BindingSetDescription viewProjSetDesc;
        viewProjSetDesc.PushConstantBuffer(0, m_memoryPool->GetBuffer(viewProjAllocation.chunkIndex).GetPtr(), render::ShaderType_Vertex, render::BufferRange(viewProjAllocation.pointer, viewProjAllocation.size));
        render::BindingSetHandle viewProjSet = m_bindingCache->GetCachedBindingSet(viewProjSetDesc);
        
        
        if (m_transforms.count < list.modelInstances.count)
        {
            m_transforms.Release();
            m_transforms.Reserve(list.modelInstances.count);
        }
        uint64_t transformSize = m_transforms.count * sizeof(glm::mat4);
        ShaderMemoryPool::MemoryAllocation transformAllocation = m_memoryPool->Allocate("transforms", transformSize);
        render::BindingSetDescription transformSetDesc;
        transformSetDesc.PushVolatileConstantBuffer(0, m_memoryPool->GetBuffer(transformAllocation.chunkIndex).GetPtr(), render::ShaderType_Vertex, render::BufferRange(transformAllocation.pointer, sizeof(glm::mat4)/*transformAllocation.size*/));
        render::BindingSetHandle transformSet = m_bindingCache->GetCachedBindingSet(transformSetDesc);

        render::GraphicsState state;
        state.rt = rt;

        for (uint32_t i = 0; i < list.modelInstances.count; ++i)
        {
            ModelInstance& instance = list.modelInstances.data[i];
            check(instance.model < list.models.count);
            Model& model = list.models.data[instance.model];

            static constexpr float dt = 0.033f;
            Mist::tAngles rot(90.f * dt * static_cast<float>(rand() % 100)*0.01f, 45.f * dt * static_cast<float>(rand() % 100)*0.01f, 0.f);
            instance.transform = instance.transform * rot.ToMat4();
            
            m_transforms.data[i] = instance.transform;
            for (uint32_t j = 0; j < model.meshes.count; ++j)
            {
                Mesh& mesh = model.meshes.data[j];
                uint32_t lastMaterial = UINT32_MAX;
                state.indexBuffer = mesh.ib;
                state.vertexBuffer = mesh.vb;
                psoDesc.vertexInputLayout = mesh.inputLayout;
                for (uint32_t k = 0; k < mesh.primitives.count; ++k)
                {
                    Primitive& primitive = mesh.primitives.data[k];
                    if (lastMaterial != primitive.material)
                    {
                        lastMaterial = primitive.material;
                        Material& material = mesh.materials.data[primitive.material];
                        psoDesc.vertexShader = material.vs;
                        psoDesc.fragmentShader = material.fs;

                        char buff[32];
                        sprintf_s(buff, "%p", &material);
                        ShaderMemoryPool::MemoryAllocation materialAllocation = m_memoryPool->Allocate(buff, sizeof(material.color));
                        m_memoryPool->Write(materialAllocation, &material.color, sizeof(material.color));

                        render::BindingSetDescription desc;
                        desc.PushConstantBuffer(0, m_memoryPool->GetBuffer(materialAllocation.chunkIndex).GetPtr(), render::ShaderType_Fragment, render::BufferRange(materialAllocation.pointer, materialAllocation.size));
                        render::BindingSetHandle set = m_bindingCache->GetCachedBindingSet(desc);

                        psoDesc.bindingLayouts.Clear();
                        psoDesc.bindingLayouts.Push(viewProjSet->m_layout);
                        psoDesc.bindingLayouts.Push(transformSet->m_layout);
                        psoDesc.bindingLayouts.Push(set->m_layout);
                        state.bindings.Clear();
                        state.bindings.SetBindingSlot(0, viewProjSet);
                        uint32_t bindingOffset = m_device->AlignUniformSize(i * sizeof(glm::mat4));
                        state.bindings.SetBindingSlot(1, transformSet, &bindingOffset, 1);
                        state.bindings.SetBindingSlot(2, set);

                        state.pipeline = GetPso(psoDesc, state.rt);
                        m_cmd->SetGraphicsState(state);
                    }

                    m_cmd->DrawIndexed(primitive.count, 1, primitive.first, 0, 0);
                }
            }
        }

        // flush transform buffer
        if (m_transforms.count)
            m_memoryPool->Write(transformAllocation, m_transforms.data, m_transforms.count * sizeof(glm::mat4));
    }

    void RenderSystem::InitScreenQuad()
    {
        m_cmd->BeginRecording();

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
        m_cmd->WriteBuffer(m_screenQuadCopy.vb, quadVertices, sizeof(quadVertices));

        // Index buffer
        bufferDescription.size = sizeof(quadIndices);
        bufferDescription.bufferUsage = render::BufferUsage_IndexBuffer;
        m_screenQuadCopy.ib = m_device->CreateBuffer(bufferDescription);
        m_cmd->WriteBuffer(m_screenQuadCopy.ib, quadIndices, sizeof(quadIndices));

        m_cmd->EndRecording();
        uint64_t submissionId = m_cmd->ExecuteCommandList();
        check(m_device->WaitForSubmissionId(submissionId));

        // Sampler
        render::SamplerDescription samplerDesc;
        samplerDesc.minFilter = render::Filter_Linear;
        samplerDesc.magFilter = render::Filter_Linear;
        m_screenQuadCopy.sampler = m_device->CreateSampler(samplerDesc);

        // Shader modules
        render::shader_compiler::CompiledBinary vsSrc = render::shader_compiler::Compile(Mist::cAssetPath("shaders/quad.vert"), render::ShaderType_Vertex);
        render::shader_compiler::CompiledBinary fsSrc = render::shader_compiler::Compile(Mist::cAssetPath("shaders/quad.frag"), render::ShaderType_Fragment);

        m_screenQuadCopy.vs = m_device->CreateShader({ .type = render::ShaderType_Vertex }, vsSrc.binary, vsSrc.binaryCount);
        m_screenQuadCopy.fs = m_device->CreateShader({ .type = render::ShaderType_Fragment }, fsSrc.binary, fsSrc.binaryCount);
    }

    void RenderSystem::DestroyScreenQuad()
    {
        m_screenQuadCopy.fs = nullptr;
        m_screenQuadCopy.vs = nullptr;
        m_screenQuadCopy.vb = nullptr;
        m_screenQuadCopy.ib = nullptr;
        m_screenQuadCopy.sampler = nullptr;
    }

    void RenderSystem::CopyToPresentRt(render::TextureHandle texture)
    {
        m_cmd->ClearState();

        //render::TextureBarrier barriers[2];
        //barriers[0].texture = m_presentRts[m_swapchainIndex]->m_description.colorAttachments[0].texture;
        //barriers[0].newLayout = render::ImageLayout_ColorAttachment;
        //barriers[1].texture = texture;
        //barriers[1].newLayout = render::ImageLayout_ShaderReadOnly;
        //m_cmd->SetTextureState(barriers, 2);

        // Set
        render::BindingSetDescription setDesc;
        setDesc.PushTextureSRV(0, texture.GetPtr(), m_screenQuadCopy.sampler, render::ShaderType_Fragment);
        render::BindingSetHandle set = m_bindingCache->GetCachedBindingSet(setDesc);

        // Input layout
        render::VertexInputAttribute attributes[2] = { render::Format_R32G32B32_SFloat, render::Format_R32G32_SFloat };
        render::VertexInputLayout inputLayout = render::VertexInputLayout::BuildVertexInputLayout(attributes, Mist::CountOf(attributes));

        render::GraphicsPipelineDescription psoDesc;
        psoDesc.bindingLayouts.Push(set->m_layout);
        psoDesc.vertexInputLayout = inputLayout;
        psoDesc.vertexShader = m_screenQuadCopy.vs;
        psoDesc.fragmentShader = m_screenQuadCopy.fs;
        psoDesc.renderState.viewportState.viewport = m_presentRts[m_swapchainIndex]->m_info.GetViewport();
        psoDesc.renderState.viewportState.scissor = m_presentRts[m_swapchainIndex]->m_info.GetScissor();
        psoDesc.renderState.blendState.renderTargetBlendStates.Push(render::RenderTargetBlendState());

        render::GraphicsPipelineHandle pso = GetPso(psoDesc, m_presentRts[m_swapchainIndex]);

        render::GraphicsState state;
        state.pipeline = pso;
        state.vertexBuffer = m_screenQuadCopy.vb;
        state.indexBuffer = m_screenQuadCopy.ib;
        state.rt = m_presentRts[m_swapchainIndex];
        state.bindings.SetBindingSlot(0, set);
        m_cmd->SetGraphicsState(state);
        m_cmd->DrawIndexed(6, 1, 0, 0, 0);
        m_cmd->ClearState();

        //barriers[0].texture = m_presentRts[m_swapchainIndex]->m_description.colorAttachments[0].texture;
        //barriers[0].newLayout = render::ImageLayout_PresentSrc;
        //barriers[1].texture = texture;
        //barriers[1].newLayout = render::ImageLayout_ColorAttachment;
        //m_cmd->SetTextureState(barriers, 2);
    }

    void RenderSystem::ImGuiDraw()
    {
        //ImGui::SetNextWindowPos(ImVec2(500, 500));
        ImGui::Begin("Render system");
        ImGui::Text("Tris:          %4d", m_cmd->GetStats().tris);
        ImGui::Text("DrawCalls:     %4d", m_cmd->GetStats().drawCalls);
        ImGui::Text("Pipelines:     %4d", m_cmd->GetStats().pipelines);
        ImGui::Text("Rts:           %4d", m_cmd->GetStats().rts);
        const render::MemoryContext& memstats = m_device->GetContext().memoryContext;
        ImGui::Text("Buffers:       %4d (%6d b/%6d b)", memstats.bufferStats.allocationCounts,
            memstats.bufferStats.currentAllocated, memstats.bufferStats.maxAllocated);
        ImGui::Text("Images:        %4d (%6d b/%6d b)", memstats.imageStats.allocationCounts,
            memstats.imageStats.currentAllocated, memstats.imageStats.maxAllocated);
        const render::CommandQueue* queue = m_device->GetCommandQueue(render::Queue_Graphics);
        ImGui::Text("CB total:      %4d", queue->GetTotalCommandBuffers());
        ImGui::Text("CB submitted:  %4d", queue->GetSubmittedCommandBuffersCount());
        ImGui::Text("CB pool:       %4d", queue->GetPoolCommandBuffersCount());
        ImGui::End();
    }

    void RenderSystem::BeginFrame()
    {
        m_frame++;

        ui::BeginFrame();
        ImGuiDraw();
        m_cmd->ResetStats();

        render::CommandQueue* commandQueue = m_device->GetCommandQueue(render::Queue_Graphics);

        render::SemaphoreHandle presentSemaphore = m_frameSyncronization[GetFrameIndex()].presentSemaphore;
        render::SemaphoreHandle renderSemaphore = m_frameSyncronization[GetFrameIndex()].renderQueueSemaphore;

        if (m_frameSyncronization[GetFrameIndex()].submission)
            m_device->WaitForSubmissionId(m_frameSyncronization[GetFrameIndex()].submission);

        commandQueue->ProcessInFlightCommands();
        m_swapchainIndex = m_device->AcquireSwapchainIndex(presentSemaphore);
        commandQueue->AddWaitSemaphore(presentSemaphore, 0);
        commandQueue->AddSignalSemaphore(renderSemaphore, 0);

        m_cmd->BeginRecording();
    }

    void RenderSystem::EndFrame()
    {
        ui::EndFrame(m_cmd);

        CopyToPresentRt(m_ldrTexture);
        m_cmd->SetTextureState(render::TextureBarrier{ m_presentRts[m_swapchainIndex]->m_description.colorAttachments[0].texture, render::ImageLayout_PresentSrc });
        m_cmd->EndRecording();

        uint64_t submissionId = m_cmd->ExecuteCommandList();
        m_device->Present(m_frameSyncronization[GetFrameIndex()].renderQueueSemaphore);

        m_memoryPool->Submit(submissionId);
        m_frameSyncronization[GetFrameIndex()].submission = submissionId;
    }
}
