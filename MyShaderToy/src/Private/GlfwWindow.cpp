#include "GlfwWindow.h"

GlfwWindow::GlfwWindow()
{
    m_window = nullptr;

    glfwInit();
}

const char** GlfwWindow::getRequiredInstanceExtesions(uint32_t& extensionsCount)
{
    const char**  extensionsNames = glfwGetRequiredInstanceExtensions(&extensionsCount);

    return extensionsNames;
}

bool GlfwWindow::createWindow(const std::string& WindowName, int width, int height)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, WindowName.c_str(), nullptr, nullptr);

    return m_window != nullptr;
}

VkResult GlfwWindow::createSurface(VkInstance& instance, VkSurfaceKHR& vkSurface) const
{
    return glfwCreateWindowSurface(instance, m_window, nullptr, &vkSurface);
}

GLFWwindow* GlfwWindow::getWindow() const
{
    return m_window;
}

int GlfwWindow::getWidth() const
{
    int width;
    glfwGetWindowSize(m_window, &width, nullptr);
    return width;
}

int GlfwWindow::getHeight() const
{
    int height;
    glfwGetWindowSize(m_window, nullptr, &height);
    return height;
}

bool GlfwWindow::ShouldClose() const
{
    if (!m_window)
        return false;

    return glfwWindowShouldClose(m_window);
}

void GlfwWindow::destroyWindow() const
{
    glfwDestroyWindow(m_window);
}

void GlfwWindow::terminateGlfw()
{
    glfwTerminate();
}