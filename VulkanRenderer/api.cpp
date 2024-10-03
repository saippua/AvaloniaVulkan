#include "api.hh"

extern "C" {

SHAREDVULKAN_API Renderer *initEngine(DebugCallback debugCallback)
{
  try {
    Renderer *vulkan = new Renderer(debugCallback);
    return vulkan;
  }
  catch (std::exception) {
    return nullptr;
  }
}

SHAREDVULKAN_API int attachRenderer(void *ptr, HWND handle)
{
  Renderer *vulkan = static_cast<Renderer *>(ptr);

  SurfaceInfo surfaceInfo {};
  surfaceInfo.height = 0; // We need to get these from Avalonia
  surfaceInfo.width = 0;

  surfaceInfo.hwnd = handle;
  surfaceInfo.hinstance = GetModuleHandle(nullptr);
  surfaceInfo.glfwExtensions = { "VK_KHR_surface", "VK_KHR_win32_surface" };

  vulkan->attach(surfaceInfo);

  return 0;
}

SHAREDVULKAN_API int detachRenderer(void *ptr, HWND handle)
{
  Renderer *vulkan = static_cast<Renderer *>(ptr);
  vulkan->detach();
  return 0;
}

SHAREDVULKAN_API int destroyEngine(void *ptr) {
  Renderer *vulkan = static_cast<Renderer*>(ptr);
  delete vulkan;
  return 0;
}


SHAREDVULKAN_API bool setPerformanceMonitorCallback(void *ptr, PerformanceMonitorCallback callback)
{
  Renderer *vulkan = static_cast<Renderer *>(ptr);
  try {
    vulkan->setMonitorCallback(callback);
    return true;
  }
  catch (std::exception) {
    return false;
  }
}

SHAREDVULKAN_API bool setSimpleCallback(void *ptr, SimpleCallback callback)
{
  Renderer *vulkan = static_cast<Renderer *>(ptr);
  try {
    vulkan->setSimpleCallback(callback);
    return true;
  }
  catch (std::exception) {
    return false;
  }
}
}
