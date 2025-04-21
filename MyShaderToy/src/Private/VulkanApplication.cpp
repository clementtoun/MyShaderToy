#include "VulkanApplication.h"

VulkanApplication::VulkanApplication(const std::string& ApplicationName, uint32_t ApplicationVersion,
    const std::string& EngineName, uint32_t EngineVersion, int width, int height)
{
    if (!m_GlfwWindow.createWindow(ApplicationName, width, height))
        throw std::runtime_error("GLFW Window creation failed !");

    m_VulkanCore.SetWindow(&m_GlfwWindow);

    m_VulkanCore.CreateDevice(ApplicationName, ApplicationVersion, EngineName, EngineVersion);

    m_VulkanCore.CreateSwapChain();

    m_VulkanCore.GetQueues();

    m_VulkanCore.CreateGraphicPipeline();

    m_VulkanCore.CreateCommandPool();

    m_VulkanCore.CreateCommandBuffer();

    m_VulkanCore.CreateSyncObject();

    m_VulkanCore.CreateVmaAllocator();

    m_VulkanCore.InitImGui();

    m_VulkanCore.CreateRenderTarget(width, height);
}

void VulkanApplication::run()
{
    while (!m_GlfwWindow.ShouldClose())
    {
        glfwPollEvents();

        m_VulkanCore.Draw();
    }
}

VulkanApplication::~VulkanApplication()
{
    m_GlfwWindow.destroyWindow();
    GlfwWindow::terminateGlfw();
}


