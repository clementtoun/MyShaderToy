#pragma once

#include <vulkan/vulkan.h>
#include <VkBootstrap/VkBootstrap.h>
#include <vma/vk_mem_alloc.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_stdlib.h>
#include <glm/glm.hpp>
#include "GlfwWindow.h"
#include "ImGuiGlslEditor.h"
#include "log.h"
#include <chrono>
#include <vector>
#include <iostream>
#define NOMINMAX
#include <Windows.h>

constexpr auto MAX_FRAMES_IN_FLIGHT = 3;

#define RT_IMAGE_FORMAT VK_FORMAT_R32G32B32A32_SFLOAT

#define PRESENT_MODE VK_PRESENT_MODE_FIFO_KHR

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;

inline void CompileShaderCommand(const std::string& command)
{
    std::wstring cmd = L"cmd.exe /C " + std::wstring(command.begin(), command.end());

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;

    // Important : buffer modifiable pour CreateProcessW
    std::wstring mutable_cmd = cmd;

    if (CreateProcessW(
        NULL,
        &mutable_cmd[0],
        NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL,
        &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        DWORD error = GetLastError();
        std::wcerr << L"CreateProcessW failed with error code: " << error << std::endl;
    }
}

#else
constexpr bool enableValidationLayers = true;

inline void CompileShaderCommand(const std::string& command)
{
    std::system(command.c_str());
}

#endif

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			debug_log("Detected Vulkan error: " << err);            \
			abort();                                                \
		}                                                           \
	} while (0)

struct ImageData
{
    VkImage Image = VK_NULL_HANDLE;
    VmaAllocation ImageAllocation;
    VkImageView ImageView = VK_NULL_HANDLE;

    void Clenup(VmaAllocator allocator, vkb::DispatchTable disp)
    {
        if (ImageView != VK_NULL_HANDLE)
            disp.destroyImageView(ImageView, nullptr);

        if (Image != VK_NULL_HANDLE)
        {
            disp.destroyImage(Image, nullptr);
            vmaFreeMemory(allocator, ImageAllocation);
        }
    }
};

struct PushConstants {
    glm::vec3 iResolution;    // 12 bytes
    float iTime;              // 4 bytes
    float iTimeDelta;         // 4 bytes
    float iFrameRate;         // 4 bytes
    uint32_t iFrame;               // 4 bytes
    float padding0;           // 4 bytes (pour aligner les vec4 à 16 bytes)
    glm::vec4 iMouse;         // 16 bytes
    glm::vec4 iDate;          // 16 bytes
};

class VulkanCore
{
public:

    void SetWindow(GlfwWindow* window);

    void CreateDevice(const std::string& ApplicationName, uint32_t ApplicationVersion, const std::string& EngineName, uint32_t EngineVersion);

    void CreateSwapChain();

    void RecreateSwapChain();

    void GetQueues();

    void CreateCommandPool();

    void CreateVmaAllocator();

    void CompileShader(const std::string& spv_path);

    VkShaderModule createModule(const std::string& spv_path);

    void CreateGraphicPipeline();

    void CreateCommandBuffer();

    void CreateSyncObject();

    void CreateRenderTarget(uint32_t width, uint32_t height);

    void RecreateRenderTarget(uint32_t width, uint32_t height);

    void InitImGui();

    void RecordCommandBuffer(uint32_t imageIndex, ImDrawData* imGui_draw_data);

    void ReloadShader();

    void Draw();

    ~VulkanCore();

private:

    void GetPushConstant(PushConstants& pushConstant);

    vkb::Instance m_Instance;
    vkb::InstanceDispatchTable m_Inst_disp;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    vkb::Device m_Device;
    vkb::DispatchTable m_Disp;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_TranferQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkCommandPool m_GraphicPool = VK_NULL_HANDLE;
    VkCommandPool m_TransferPool = VK_NULL_HANDLE;
    GlfwWindow* m_Window;

    VmaAllocator m_Allocator;

    VkPipelineLayout m_GraphicPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_GraphicPipeline = VK_NULL_HANDLE;

    vkb::Swapchain m_Swapchain;
    std::vector<VkImage> m_SwapchainImages;
    std::vector<VkImageView> m_SwapchainImageViews;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<ImageData> m_RTImages;
    uint32_t m_RTWidth, m_RTHeight = 1u;

    ImGuiIO* m_io;
    VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_ImGuiDescriptors;
    VkSampler m_ImGuiSampler;

    uint32_t m_CurrentFrame = 0;
    uint32_t m_FrameCount = 0;

    float m_fps = -1.;
    float m_DeltaTime = 0;
    std::chrono::steady_clock::time_point m_LastTime;
    bool m_Playing = true;
    double m_SimulationTime = 0;

    glm::vec2 m_CurrentMousePose = glm::vec2(0.);
    glm::vec2 m_LastClickMousePose = glm::vec2(0.);
    bool m_MouseDown = false;

    ImGuiGlslEditor m_ImGuiGlslEditor;

    const std::vector<std::string> wantedLayers = { "VK_LAYER_LUNARG_monitor" };
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
};
