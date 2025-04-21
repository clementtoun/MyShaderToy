#pragma once

#include <iostream>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

class GlfwWindow
{
public:
    GlfwWindow();

    static const char** getRequiredInstanceExtesions(uint32_t& extensionsCount);

    bool createWindow(const std::string& WindowName, int width = 1080, int height = 720);

    VkResult createSurface(VkInstance& instance, VkSurfaceKHR& vkSurface) const;

    GLFWwindow* getWindow() const;

    int getWidth() const;
    int getHeight() const;

    bool ShouldClose() const;

    void destroyWindow() const;

    static void terminateGlfw();

private:
    GLFWwindow* m_window;
};
