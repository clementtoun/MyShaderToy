#pragma once

#include <iostream>
#include "GlfwWindow.h"
#include "VulkanCore.h"

class VulkanApplication
{
    
public:
    explicit VulkanApplication(const std::string& ApplicationName = "DefaultApplication", uint32_t ApplicationVersion = 0, const std::string& EngineName = "DefaultEngine", uint32_t EngineVersion = 0, int width = 1080, int height = 720);

    void run();
    
    ~VulkanApplication();
    
private:
    GlfwWindow m_GlfwWindow;
    VulkanCore m_VulkanCore;
};
