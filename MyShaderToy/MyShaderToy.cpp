#define VMA_IMPLEMENTATION

#include "VulkanApplication.h"

int main(int argc, char* argv[])
{
    try
    {
        VulkanApplication VulkanApp("MyShaderToy", 0, "MyEngine", 0, 1080, 720);
        VulkanApp.run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << '\n';
        exit(-1);
    }
    
    return 0;
}
