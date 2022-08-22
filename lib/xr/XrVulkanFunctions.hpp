#pragma once

#include "common.h"
#include "openxr/openxr.h"
#include "aurora/xr/openxr_platform.hpp"

namespace aurora::xr {

struct XrVulkanFunctions {
  void LoadInstanceProcs(XrInstance instance);

  PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR;
  PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2KHR;
  PFN_xrCreateVulkanDeviceKHR CreateVulkanDeviceKHR;
  PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR;
};
}