#include "BackendBinding.hpp"

#include "../internal.hpp"
#include "../webgpu/gpu.hpp"
#include "aurora/xr/openxr_platform.hpp"
#include "../xr/common.h"
#include "aurora/xr/xr.hpp"
#include "../xr/XrVulkanFunctions.hpp"
#include "../xr/vulkan/XrSwapChainImplVk.h"
#include <iostream>

#include <SDL_vulkan.h>
#include <dawn/native/VulkanBackend.h>
#include <dawn/common/SwapChainUtils.h>

#define CHECK_VKCMD(cmd) CheckVkResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_VKRESULT(res, cmdStr) CheckVkResult(res, cmdStr, FILE_AND_LINE);

#define GET_VK_FUN(name) reinterpret_cast<PFN_vk##name>(dawn::native::vulkan::GetInstanceProcAddr(m_device, "vk" #name))

namespace aurora::webgpu::utils {
using aurora::webgpu::g_device;
namespace dvulkan = dawn::native::vulkan;

static Module Log("aurora::webgpu::utils::VulkanBinding");

static std::string vkResultString(VkResult res) {
  switch (res) {
  case VK_SUCCESS:
    return "SUCCESS";
  case VK_NOT_READY:
    return "NOT_READY";
  case VK_TIMEOUT:
    return "TIMEOUT";
  case VK_EVENT_SET:
    return "EVENT_SET";
  case VK_EVENT_RESET:
    return "EVENT_RESET";
  case VK_INCOMPLETE:
    return "INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_SURFACE_LOST_KHR:
    return "ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR:
    return "SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR:
    return "ERROR_OUT_OF_DATE_KHR";
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
  case VK_ERROR_VALIDATION_FAILED_EXT:
    return "ERROR_VALIDATION_FAILED_EXT";
  case VK_ERROR_INVALID_SHADER_NV:
    return "ERROR_INVALID_SHADER_NV";
  default:
    return std::to_string(res);
  }
}

inline void ThrowVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
  xr::Throw(xr::Fmt("VkResult failure [%s]", vkResultString(res).c_str()), originator, sourceLocation);
}

inline VkResult CheckVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
  if ((res) < VK_SUCCESS) {
    ThrowVkResult(res, originator, sourceLocation);
  }

  return res;
}

inline void ThrowIfFailed(VkResult res) {
  if (res != VK_SUCCESS)
    Log.report(LOG_FATAL, FMT_STRING("{}\n"), res);
}

class VulkanBinding : public BackendBinding {
public:
  VulkanBinding(SDL_Window* window, WGPUDevice device) : BackendBinding(window, device) {}

  virtual WGPUTextureFormat GetPreferredSwapChainTextureFormat() {
    if (m_swapChainImpl.userData == nullptr) {
      CreateSwapChainImpl();
    }
    return dawn::native::vulkan::GetNativeSwapChainPreferredFormat(&m_swapChainImpl);
  }

  uint64_t GetSwapChainImplementation() {
    if (m_swapChainImpl.userData == nullptr) {
      CreateSwapChainImpl();
    }
    return reinterpret_cast<uint64_t>(&m_swapChainImpl);
  }

protected:
  DawnSwapChainImplementation m_swapChainImpl{};
  virtual void CreateSwapChainImpl() {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    ASSERT(SDL_Vulkan_CreateSurface(m_window, dawn::native::vulkan::GetInstance(m_device), &surface),
           "Failed to create Vulkan surface: {}", SDL_GetError());
    m_swapChainImpl = dawn::native::vulkan::CreateNativeSwapChainImpl(m_device, surface);
  }
};

class XrVulkanBinding : public VulkanBinding {
public:
  XrVulkanBinding(SDL_Window* window, WGPUDevice device) : VulkanBinding(window, device) {
    buildXrGraphicsBinding();
  }

  WGPUTextureFormat GetPreferredSwapChainTextureFormat() override {
    if (m_swapChainImpl.userData == nullptr) {
      CreateSwapChainImpl();
    }

    xr::XrSwapChainImplVk* impl = reinterpret_cast<xr::XrSwapChainImplVk*>(m_swapChainImpl.userData);
    return static_cast<WGPUTextureFormat>(impl->GetPreferredFormat());
  }

private:
  void buildXrGraphicsBinding(){

    VkInstance vkInstance = dawn::native::vulkan::GetInstance(m_device);
    VkPhysicalDevice vkPhysicalDevice = dawn::native::vulkan::GetPhysicalDevice(m_device);
    VkDevice vkDevice = dawn::native::vulkan::GetDevice(m_device);
    int queueFamily = dawn::native::vulkan::GetQueueGraphicsFamily(m_device);

    m_xrGraphicsBinding.instance = vkInstance;
    m_xrGraphicsBinding.physicalDevice = vkPhysicalDevice;
    m_xrGraphicsBinding.device = vkDevice;
    m_xrGraphicsBinding.queueFamilyIndex = queueFamily;
    m_xrGraphicsBinding.queueIndex = 0;
  }

  void CreateSwapChainImpl() override {
//    VkSurfaceKHR surface = VK_NULL_HANDLE;
//    ASSERT(SDL_Vulkan_CreateSurface(m_window, dawn::native::vulkan::GetInstance(m_device), &surface),
//           "Failed to create Vulkan surface: {}", SDL_GetError());
//    m_swapChainImpl = dawn::native::vulkan::CreateNativeSwapChainImpl(m_device, surface);
    m_swapChainImpl = CreateSwapChainImplementation(new xr::XrSwapChainImplVk());
    m_swapChainImpl.textureUsage = WGPUTextureUsage_Present;
  }
};

BackendBinding* CreateVulkanBinding(SDL_Window* window, WGPUDevice device) {
  return new VulkanBinding(window, device);
}

BackendBinding* CreateXrVulkanBinding(SDL_Window* window, WGPUDevice device){
  return new XrVulkanBinding(window, device);
}

}; // namespace aurora::webgpu::utils
