#include "UI.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
#include "Render/Shader.h"
#include "Render/Globals.h"
#include "Render/VulkanRenderEngine.h"
#include "Application/Application.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include "Core/Console.h"
#include "Utils/FileSystem.h"
#include "Render/RenderContext.h"
#include "Render/RendererBase.h"

#include "RenderAPI/Device.h"

namespace rendersystem
{
    namespace ui
    {
        Mist::CIntVar CVar_ShowImGuiDemo("ui_showdemo", 0);

        void ExecCommand_ActivateMultipleViewports(const char* cmd)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags& ImGuiConfigFlags_ViewportsEnable ?
                io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable
                : io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        void PushStyle()
        {
#if 0
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowMinSize = ImVec2(160, 20);
            style.FramePadding = ImVec2(4, 2);
            style.ItemSpacing = ImVec2(6, 2);
            style.ItemInnerSpacing = ImVec2(6, 4);
            style.Alpha = 0.95f;
            style.WindowRounding = 4.0f;
            style.FrameRounding = 2.0f;
            style.IndentSpacing = 6.0f;
            style.ItemInnerSpacing = ImVec2(2, 4);
            style.ColumnsMinSpacing = 50.0f;
            style.GrabMinSize = 14.0f;
            style.GrabRounding = 16.0f;
            style.ScrollbarSize = 12.0f;
            style.ScrollbarRounding = 16.0f;

            style.Colors[ImGuiCol_Text] = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);
            style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
            style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
            style.Colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
            style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
            style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
            style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
            style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
            style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_Header] = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
            style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
            style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
            style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
            style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
            style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_PlotLines] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
            style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
            style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
            style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
            style.Colors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.9f);
            //style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.22f, 0.27f, 0.73f); 
#else
            auto& colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

            // Headers
            colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
            colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
            colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

            // Buttons
            colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
            colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
            colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

            // Frame BG
            colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
            colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
            colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

            // Tabs
            colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
            colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
            colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
            colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

            // Title
            colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
            colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
#endif // 0

        }

        class ImGuiInstance
        {
        public:
            void Init(render::Device* device, render::RenderTargetHandle rt, void* windowHandle)
            {
                m_device = device;
                m_rt = rt;
                check(m_device && m_rt);
                Mist::AddConsoleCommand("ui_multiviewports", &ExecCommand_ActivateMultipleViewports);

                // Create descriptor pool for imgui
                VkDescriptorPoolSize poolSizes[] =
                {
                    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
                };
                VkDescriptorPoolCreateInfo poolInfo = {};
                poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                poolInfo.pNext = nullptr;
                poolInfo.maxSets = 1000;
                poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
                poolInfo.pPoolSizes = poolSizes;
                poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

                vkcheck(vkCreateDescriptorPool(m_device->GetContext().device, &poolInfo, m_device->GetContext().allocationCallbacks, &m_imguiPool));
                m_device->SetDebugName(m_imguiPool, "ImGui_DescriptorPool", VK_OBJECT_TYPE_DESCRIPTOR_POOL);

                // Init imgui lib
                ImGui::CreateContext();
                ImGui_ImplSDL2_InitForVulkan((SDL_Window*)windowHandle);
                ImGui_ImplVulkan_InitInfo initInfo
                {
                    .Instance = m_device->GetContext().instance,
                    .PhysicalDevice = m_device->GetContext().physicalDevice,
                    .Device = m_device->GetContext().device,
                    .Queue = m_device->GetCommandQueue(render::Queue_Graphics)->m_queue,
                    .DescriptorPool = m_imguiPool,
                    .RenderPass = rt->m_renderPass,
                    .MinImageCount = 3,
                    .ImageCount = 3,
                    .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                    .Subpass = 0,
                };
                ImGui_ImplVulkan_Init(&initInfo);
#if 0

                // Execute gpu command to upload imgui font textures
                utils::CmdSubmitTransfer(context,
                    [&](VkCommandBuffer cmd)
                    {
                        ImGui_ImplVulkan_CreateFontsTexture(cmd);
                    });
                ImGui_ImplVulkan_DestroyFontUploadObjects();
#endif // 0

                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
                io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

                Mist::cAssetPath fontPath("fonts/mono/cascadiamono.ttf");
                io.Fonts->AddFontFromFileTTF(fontPath, 12.f);
                io.Fonts->Build();

                ImGuiStyle& style = ImGui::GetStyle();
                PushStyle();
                ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, (float)(int)(style.FramePadding.y * 0.60f));
                ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, (float)(int)(style.ItemSpacing.y * 0.60f));
            }

            void Draw(render::CommandListHandle cmd)
            {
                CPU_PROFILE_SCOPE(ImGuiPass);
                //BeginGPUEvent(context, cmd, "ImGui");

                //Renderer* renderer = context.Renderer;
                //CommandList* commandList = context.CmdList;
                //commandList->SetGraphicsState({ .Rt = &renderer->GetLDRTarget() });
                ////renderer->GetLDRTarget().BeginPass(context, context.GetFrameContext().GraphicsCommandContext.CommandBuffer);
                //commandList->BeginMarker("ImGui");
                //ImGuiInstance.Draw(context, commandList->GetCurrentCommandBuffer()->CmdBuffer);
                //commandList->ClearState();
                //commandList->EndMarker();
                //renderer->GetLDRTarget().EndPass(context.GetFrameContext().GraphicsCommandContext.CommandBuffer);

                cmd->BeginMarker("ImGui");
                render::GraphicsState state{};
                state.rt = m_rt;
                cmd->ClearState();
                cmd->SetGraphicsState(state);

                ImGui::Render();
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->GetCommandBuffer()->cmd);
                if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                {
                    ImGui::UpdatePlatformWindows();
                    ImGui::RenderPlatformWindowsDefault();
                }
                cmd->ClearState();
                cmd->EndMarker();
                //EndGPUEvent(context, cmd);
            }

            void Destroy()
            {
                ImGui_ImplVulkan_Shutdown();
                vkDestroyDescriptorPool(m_device->GetContext().device, m_imguiPool, m_device->GetContext().allocationCallbacks);
                m_imguiPool = VK_NULL_HANDLE;
                m_rt = nullptr;
                m_device = nullptr;
            }

            void BeginFrame()
            {
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL2_NewFrame(/*(SDL_Window*)context.Window->WindowInstance*/);
                ImGui::NewFrame();
            }
        private:
            VkDescriptorPool m_imguiPool= VK_NULL_HANDLE;
            render::Device* m_device = nullptr;
            render::RenderTargetHandle m_rt = nullptr;
        };

        ImGuiInstance g_imgui;

        void Init(render::Device* device, render::RenderTargetHandle rt, void* windowHandle)
        {
            check(device && rt && windowHandle);
            g_imgui.Init(device, rt, windowHandle);
        }

        void Destroy()
        {
            g_imgui.Destroy();
        }

        void BeginFrame()
        {
            CPU_PROFILE_SCOPE(UI_Begin);
            g_imgui.BeginFrame();
        }

        void EndFrame(render::CommandListHandle cmd)
        {
            if (CVar_ShowImGuiDemo.Get())
                ImGui::ShowDemoWindow();

            g_imgui.Draw(cmd);
            //Renderer* renderer = context.Renderer;
            //CommandList* commandList = context.CmdList;
            //commandList->SetGraphicsState({ .Rt = &renderer->GetLDRTarget() });
            ////renderer->GetLDRTarget().BeginPass(context, context.GetFrameContext().GraphicsCommandContext.CommandBuffer);
            //commandList->BeginMarker("ImGui");
            //ImGuiInstance.Draw(context, commandList->GetCurrentCommandBuffer()->CmdBuffer);
            //commandList->ClearState();
            //commandList->EndMarker();
            //renderer->GetLDRTarget().EndPass(context.GetFrameContext().GraphicsCommandContext.CommandBuffer);
        }
    }

}
