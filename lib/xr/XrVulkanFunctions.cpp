#include "XrVulkanFunctions.hpp"
#include <openxr/openxr.h>
#include "openxr_platform.hpp"

namespace aurora::xr {
#define GET_GLOBAL_PROC(instance, name)                                                           \
  xrGetInstanceProcAddr(instance, "xr" #name, reinterpret_cast<PFN_xrVoidFunction*>(&name));      \
  if (name == nullptr) {                                                                          \
      Log.report(LOG_FATAL, FMT_STRING("Failed to get proc xr{}"), #name);                        \
  }

void XrVulkanFunctions::LoadInstanceProcs(XrInstance instance) {
  GET_GLOBAL_PROC(instance, GetVulkanGraphicsDevice2KHR);
}

}