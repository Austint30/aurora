#include "XrVulkanFunctions.hpp"
#include <openxr/openxr.h>

namespace aurora::xr {
#define GET_GLOBAL_PROC(instance, name)                                                           \
  xrGetInstanceProcAddr(instance, "xr" #name, reinterpret_cast<PFN_xrVoidFunction*>(&name));      \
  if (name == nullptr) {                                                                          \
      Log.report(LOG_FATAL, FMT_STRING("Failed to get proc xr{}"), #name);                        \
  }

void XrVulkanFunctions::LoadInstanceProcs(XrInstance instance) {
  GET_GLOBAL_PROC(instance, GetVulkanGraphicsDevice2KHR);
  GET_GLOBAL_PROC(instance, GetVulkanGraphicsRequirements2KHR);
  GET_GLOBAL_PROC(instance, CreateVulkanDeviceKHR);
  GET_GLOBAL_PROC(instance, CreateVulkanInstanceKHR);
  GET_GLOBAL_PROC(instance, CreateSwapchain);
  GET_GLOBAL_PROC(instance, EnumerateViewConfigurationViews);
  GET_GLOBAL_PROC(instance, EnumerateSwapchainImages);
  GET_GLOBAL_PROC(instance, AcquireSwapchainImage);
  GET_GLOBAL_PROC(instance, WaitSwapchainImage);
  GET_GLOBAL_PROC(instance, ReleaseSwapchainImage);
}

}