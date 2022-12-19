#include "BackendBinding.hpp"

#include "../internal.hpp"
#include "../webgpu/gpu.hpp"
#include "aurora/xr/openxr_platform.hpp"
#include "../xr/common.h"
#include "../xr/xr.hpp"
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

  WGPUTextureFormat GetPreferredSwapChainTextureFormat() {
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

  VkBool32 debugReport(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t /*location*/,
                       int32_t /*messageCode*/, const char* pLayerPrefix, const char* pMessage) {
    std::string flagNames;
    std::string objName;
    AuroraLogLevel level = LOG_ERROR;

//    if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0u) {
//      flagNames += "DEBUG:";
//      level = Log::Level::Verbose;
//    }
    if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0u) {
      flagNames += "INFO:";
      level = LOG_INFO;
    }
    if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0u) {
      flagNames += "PERF:";
      level = LOG_WARNING;
    }
    if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0u) {
      flagNames += "WARN:";
      level = LOG_WARNING;
    }
    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0u) {
      flagNames += "ERROR:";
      level = LOG_ERROR;
    }

#define LIST_OBJECT_TYPES(_) \
    _(UNKNOWN)               \
    _(INSTANCE)              \
    _(PHYSICAL_DEVICE)       \
    _(DEVICE)                \
    _(QUEUE)                 \
    _(SEMAPHORE)             \
    _(COMMAND_BUFFER)        \
    _(FENCE)                 \
    _(DEVICE_MEMORY)         \
    _(BUFFER)                \
    _(IMAGE)                 \
    _(EVENT)                 \
    _(QUERY_POOL)            \
    _(BUFFER_VIEW)           \
    _(IMAGE_VIEW)            \
    _(SHADER_MODULE)         \
    _(PIPELINE_CACHE)        \
    _(PIPELINE_LAYOUT)       \
    _(RENDER_PASS)           \
    _(PIPELINE)              \
    _(DESCRIPTOR_SET_LAYOUT) \
    _(SAMPLER)               \
    _(DESCRIPTOR_POOL)       \
    _(DESCRIPTOR_SET)        \
    _(FRAMEBUFFER)           \
    _(COMMAND_POOL)          \
    _(SURFACE_KHR)           \
    _(SWAPCHAIN_KHR)         \
    _(DISPLAY_KHR)           \
    _(DISPLAY_MODE_KHR)

    switch (objectType) {
    default:
#define MK_OBJECT_TYPE_CASE(name)                  \
    case VK_DEBUG_REPORT_OBJECT_TYPE_##name##_EXT: \
        objName = #name;                           \
        break;
      LIST_OBJECT_TYPES(MK_OBJECT_TYPE_CASE)

#if VK_HEADER_VERSION >= 46
      MK_OBJECT_TYPE_CASE(DESCRIPTOR_UPDATE_TEMPLATE_KHR)
#endif
#if VK_HEADER_VERSION >= 70
      MK_OBJECT_TYPE_CASE(DEBUG_REPORT_CALLBACK_EXT)
#endif
    }

    if ((objectType == VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT) && (strcmp(pLayerPrefix, "Loader Message") == 0) &&
        (strncmp(pMessage, "Device Extension:", 17) == 0)) {
      return VK_FALSE;
    }

    Log.report(level, FMT_STRING("{} ({} 0x{}lx) [{}] {}"), flagNames.c_str(), objName.c_str(), object, pLayerPrefix, pMessage);
    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0u) {
      return VK_FALSE;
    }
    if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0u) {
      return VK_FALSE;
    }
    return VK_FALSE;
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportThunk(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                                         uint64_t object, size_t location, int32_t messageCode,
                                                         const char* pLayerPrefix, const char* pMessage, void* pUserData) {
    return static_cast<VulkanBinding*>(pUserData)->debugReport(flags, objectType, object, location, messageCode,
                                                                      pLayerPrefix, pMessage);
  }

  const char* GetValidationLayerName() {
    VkInstance vkInstance = dawn::native::vulkan::GetInstance(m_device);
    uint32_t layerCount;
    PFN_vkEnumerateInstanceLayerProperties pfnVkEnumerateInstanceLayerProperties = GET_VK_FUN(EnumerateInstanceLayerProperties);

    pfnVkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    pfnVkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    std::vector<const char*> validationLayerNames;
    validationLayerNames.push_back("VK_LAYER_KHRONOS_validation");
    validationLayerNames.push_back("VK_LAYER_LUNARG_standard_validation");

    // Enable only one validation layer from the list above. Prefer KHRONOS.
    for (auto& validationLayerName : validationLayerNames) {
      for (const auto& layerProperties : availableLayers) {
        if (0 == strcmp(validationLayerName, layerProperties.layerName)) {
          return validationLayerName;
        }
      }
    }

    return nullptr;
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
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    ASSERT(SDL_Vulkan_CreateSurface(m_window, dawn::native::vulkan::GetInstance(m_device), &surface),
           "Failed to create Vulkan surface: {}", SDL_GetError());
//    m_swapChainImpl = dawn::native::vulkan::CreateNativeSwapChainImpl(m_device, surface);
    m_swapChainImpl = CreateSwapChainImplementation(new xr::XrSwapChainImplVk());
  }
};

BackendBinding* CreateVulkanBinding(SDL_Window* window, WGPUDevice device) {
  if (xr::g_OpenXRSessionManager->xrEnabled){
    return new XrVulkanBinding(window, device);
  }
  else
  {
    return new VulkanBinding(window, device);
  }
}

}; // namespace aurora::webgpu::utils
