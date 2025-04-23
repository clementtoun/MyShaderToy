#define VMA_IMPLEMENTATION

#include "VulkanApplication.h"
#include "log.h"
#include <windows.h>

#ifdef NDEBUG

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    try
    {
        VulkanApplication VulkanApp("MyShaderToy", 0, "MyEngine", 0, 1080, 720);
        VulkanApp.run();
    }
    catch (std::exception& e)
    {
        MessageBoxA(NULL, e.what(), "Erreur", MB_ICONERROR | MB_OK);
        exit(-1);
    }
    
    return 0;
}

#else

int main(int argc, char** argv)
{
    try
    {
        VulkanApplication VulkanApp("MyShaderToy", 0, "MyEngine", 0, 1080, 720);
        VulkanApp.run();
    }
    catch (std::exception& e)
    {
        debug_log(e.what());
        exit(-1);
    }
    
    return 0;

}


#endif
