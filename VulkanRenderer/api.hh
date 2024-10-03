#include "interop.h"
#include "renderer.hpp"

#ifdef SHAREDVULKAN_EXPORTS
#define SHAREDVULKAN_API __declspec(dllexport)
#else
#define SHAREDVULKAN_API __declspec(dllexport)
#endif

extern "C" {

  SHAREDVULKAN_API Renderer* initEngine(DebugCallback debugCallback);

  SHAREDVULKAN_API int attachRenderer(void* ptr, HWND handle);

  SHAREDVULKAN_API int detachRenderer(void* ptr, HWND handle);

  SHAREDVULKAN_API int destroyEngine(void* ptr);


  SHAREDVULKAN_API bool setPerformanceMonitorCallback(void* ptr, PerformanceMonitorCallback callback);

  SHAREDVULKAN_API bool setSimpleCallback(void* ptr, SimpleCallback callback);
}