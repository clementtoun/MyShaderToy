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
#include <chrono>
#include <vector>
#include <iostream>

#define MAX_FRAMES_IN_FLIGHT 1

#define RT_IMAGE_FORMAT VK_FORMAT_R32G32B32A32_SFLOAT

#define PRESENT_MODE VK_PRESENT_MODE_FIFO_KHR

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
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
    int iFrame;               // 4 bytes
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

    VkPipelineLayout m_GraphicPipelineLayout;
    VkPipeline m_GraphicPipeline;

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
    VkDescriptorPool m_ImGuiDescriptorPool;
    std::vector<VkDescriptorSet> m_ImGuiDescriptors;
    VkSampler m_ImGuiSampler;

    uint32_t m_CurrentFrame = 0;
    uint64_t m_FrameCount = 0;

    float m_fps = -1.;
    float m_DeltaTime = 0;
    std::chrono::steady_clock::time_point m_LastTime;
    bool m_Playing = true;
    double m_SimulationTime = 0;

    ImGuiGlslEditor m_ImGuiGlslEditor;

    const std::vector<std::string> wantedLayers = { "VK_LAYER_LUNARG_monitor" };
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
};
