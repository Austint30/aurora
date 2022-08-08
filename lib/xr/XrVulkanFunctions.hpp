#pragma once

#include "common.h"
#include "openxr/openxr.h"
#include "openxr_platform.hpp"

namespace aurora::xr {

struct XrVulkanFunctions {
  void LoadInstanceProcs(XrInstance instance);

  PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR;
};
}