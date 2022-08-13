#include "BackendBinding.hpp"

#include "../internal.hpp"
#include "../webgpu/gpu.hpp"
#include "../xr/openxr_platform.hpp"
#include "../xr/common.h"
#include "../xr/XrVulkanFunctions.hpp"
#include <iostream>

#include <SDL_vulkan.h>
#include <dawn/native/VulkanBackend.h>

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

  uint64_t GetSwapChainImplementation() override {
    if (m_swapChainImpl.userData == nullptr) {
      CreateSwapChainImpl();
    }
    return reinterpret_cast<uint64_t>(&m_swapChainImpl);
  }

  WGPUTextureFormat GetPreferredSwapChainTextureFormat() override {
    if (m_swapChainImpl.userData == nullptr) {
      CreateSwapChainImpl();
    }
    return dawn::native::vulkan::GetNativeSwapChainPreferredFormat(&m_swapChainImpl);
  }

  std::optional<XrStructureType> XrGetGraphicsBindingType() const override { return XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR; }
  std::vector<std::string> XrGetInstanceExtensions() const override { return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME}; };

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
  void XrInitializeDevice(XrInstance instance, XrSystemId systemId) override {

    m_xrVulkanFunctions.LoadInstanceProcs(instance);

    XrGraphicsRequirementsVulkan2KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    xr::CHECK_XRCMD(m_xrVulkanFunctions.GetVulkanGraphicsRequirements2KHR(instance, systemId, &graphicsRequirements));

    VkResult err;

    std::vector<const char*> layers;
#if !defined(NDEBUG)
    const char* const validationLayerName = GetValidationLayerName();
    if (validationLayerName) {
      layers.push_back(validationLayerName);
    } else {
      Log.report(LOG_WARNING, FMT_STRING("No validation layers found in the system, skipping"));
    }
#endif

    std::vector<const char*> extensions;
    extensions.push_back("VK_EXT_debug_report");
#if defined(USE_MIRROR_WINDOW)
    extensions.push_back("VK_KHR_surface");
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    extensions.push_back("VK_KHR_win32_surface");
#else
#error CreateSurface not supported on this OS
#endif  // defined(VK_USE_PLATFORM_WIN32_KHR)
#endif  // defined(USE_MIRROR_WINDOW)

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "hello_xr";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "hello_xr";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledLayerCount = (uint32_t)layers.size();
    instInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
    instInfo.enabledExtensionCount = (uint32_t)extensions.size();
    instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

    XrVulkanInstanceCreateInfoKHR createInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    createInfo.systemId = systemId;
    createInfo.pfnGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(dvulkan::GetInstanceProcAddr(m_device, "vkGetInstanceProcAddr"));
    createInfo.vulkanCreateInfo = &instInfo;
    createInfo.vulkanAllocator = nullptr;
    xr::CHECK_XRCMD(m_xrVulkanFunctions.CreateVulkanInstanceKHR(instance, &createInfo, &m_vkInstance, &err));
    CHECK_VKCMD(err);

//    auto vkCreateDebugReportCallbackEXT =
//        (PFN_vkCreateDebugReportCallbackEXT) dvulkan::GetInstanceProcAddr(m_device, "vkCreateDebugReportCallbackEXT");
//    auto vkDestroyDebugReportCallbackEXT =
//        (PFN_vkDestroyDebugReportCallbackEXT) dvulkan::GetInstanceProcAddr(m_device, "vkDestroyDebugReportCallbackEXT");
//    VkDebugReportCallbackCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT};
//    debugInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
//#if !defined(NDEBUG)
//    debugInfo.flags |=
//        VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
//#endif
//    debugInfo.pfnCallback = debugReportThunk;
//    debugInfo.pUserData = this;
//    CHECK_VKCMD(vkCreateDebugReportCallbackEXT(m_vkInstance, &debugInfo, nullptr, &m_vkDebugReporter));

    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    deviceGetInfo.systemId = systemId;
    deviceGetInfo.vulkanInstance = m_vkInstance;
    xr::CHECK_XRCMD(m_xrVulkanFunctions.GetVulkanGraphicsDevice2KHR(instance, &deviceGetInfo, &m_vkPhysicalDevice));

    VkDeviceQueueCreateInfo queueInfo = {};
    float queuePriorities[1] = {0.0};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.pNext = nullptr;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queuePriorities;
    queueInfo.queueFamilyIndex = m_queueFamilyIndex;

//    uint32_t queueFamilyCount = 0;
//    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = GET_VK_FUN(GetPhysicalDeviceQueueFamilyProperties);
//    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
//    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, &queueFamilyProps[0]);

//    Log.report(LOG_DEBUG, FMT_STRING("queueFamilyCount: {}"), queueFamilyCount);
//    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
//      // Only need graphics (not presentation) for draw queue
//      if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
//        m_queueFamilyIndex = queueInfo.queueFamilyIndex = i;
//        break;
//      }
//    }

    std::vector<const char*> deviceExtensions;

    VkPhysicalDeviceFeatures features{};
    // features.samplerAnisotropy = VK_TRUE;

#if defined(USE_MIRROR_WINDOW)
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
    deviceInfo.pEnabledFeatures = &features;

    XrVulkanDeviceCreateInfoKHR deviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    deviceCreateInfo.systemId = systemId;
    deviceCreateInfo.pfnGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(dawn::native::vulkan::GetInstanceProcAddr(m_device, "vkGetInstanceProcAddr"));
    deviceCreateInfo.vulkanCreateInfo = &deviceInfo;
    deviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
    deviceCreateInfo.vulkanAllocator = nullptr;
    xr::CHECK_XRCMD(m_xrVulkanFunctions.CreateVulkanDeviceKHR(instance, &deviceCreateInfo, &m_vkDevice, &err));
    CHECK_VKCMD(err);

    PFN_vkGetDeviceQueue vkGetDeviceQueue = GET_VK_FUN(GetDeviceQueue);
    vkGetDeviceQueue(m_vkDevice, queueInfo.queueFamilyIndex, 0, &m_vkQueue);

    //    m_memAllocator->Init(m_vkPhysicalDevice, m_vkDevice, m_device);

    m_xrGraphicsBinding.instance = m_vkInstance;
    m_xrGraphicsBinding.physicalDevice = m_vkPhysicalDevice;
    m_xrGraphicsBinding.device = m_vkDevice;
    m_xrGraphicsBinding.queueFamilyIndex = queueInfo.queueFamilyIndex;
    m_xrGraphicsBinding.queueIndex = 0;
  }
private:
  DawnSwapChainImplementation m_swapChainImpl{};
  VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  VkInstance m_vkInstance{VK_NULL_HANDLE};
  xr::XrVulkanFunctions m_xrVulkanFunctions;\
  VkDebugReportCallbackEXT m_vkDebugReporter{VK_NULL_HANDLE};
  uint32_t m_queueFamilyIndex = 0;
  VkQueue m_vkQueue{VK_NULL_HANDLE};

  void CreateSwapChainImpl() {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(m_window, dawn::native::vulkan::GetInstance(m_device), &surface) != SDL_TRUE) {
      Log.report(LOG_FATAL, FMT_STRING("Failed to create Vulkan surface: {}"), SDL_GetError());
    }
    m_swapChainImpl = dawn::native::vulkan::CreateNativeSwapChainImpl(m_device, surface);
  }
  /*
  void xrInitVulkanGraphicsBinding(XrInstance xrInstance, XrSystemId xrSystemId){

  VkInstance vkInstance = dawn::native::vulkan::GetInstance(m_device);

  // This works apparently???
  VkDevice* vkDevice = reinterpret_cast<VkDevice*>(&m_device);

  VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  deviceInfo.queueCreateInfoCount = 1;
  deviceInfo.pQueueCreateInfos = &queueInfo;
  deviceInfo.enabledLayerCount = 0;
  deviceInfo.ppEnabledLayerNames = nullptr;
  //    deviceInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
  //    deviceInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
  //    deviceInfo.pEnabledFeatures = &features;

  XrVulkanGraphicsDeviceGetInfoKHR getInfo {XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
  getInfo.systemId = xrSystemId;
  getInfo.vulkanInstance = vkInstance;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  PFN_xrGetVulkanGraphicsDevice2KHR pfnGetVulkanGraphicsDevice2KHR = nullptr;
  CHECK_XRCMD(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR",
                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDevice2KHR)));
  CHECK_XRCMD(pfnGetVulkanGraphicsDevice2KHR(xrInstance, &getInfo, &physical_device));
  XrVulkanDeviceCreateInfoKHR deviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
  deviceCreateInfo.systemId = xrSystemId;
  deviceCreateInfo.pfnGetInstanceProcAddr =
      reinterpret_cast<PFN_vkGetInstanceProcAddr>(dawn::native::vulkan::GetInstanceProcAddr);
  deviceCreateInfo.vulkanCreateInfo = &deviceInfo;
  deviceCreateInfo.vulkanPhysicalDevice = physical_device;
  deviceCreateInfo.vulkanAllocator = nullptr;
  VkResult err;

  PFN_xrCreateVulkanDeviceKHR pfnCreateVulkanDeviceKHR = nullptr;
  CHECK_XRCMD(xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanDeviceKHR",
                                    reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanDeviceKHR)));
  CHECK_XRCMD(pfnCreateVulkanDeviceKHR(xrInstance, &deviceCreateInfo, vkDevice, &err));
  ThrowIfFailed(err);

  m_xrGraphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
  m_xrGraphicsBinding.instance = vkInstance;
  m_xrGraphicsBinding.physicalDevice = m_gpus[0];
  m_xrGraphicsBinding.device = *vkDevice;
  m_xrGraphicsBinding.queueFamilyIndex = queueInfo.queueFamilyIndex;
  m_xrGraphicsBinding.queueIndex = 0;
}
    */
};

BackendBinding* CreateVulkanBinding(SDL_Window* window, WGPUDevice device) { return new VulkanBinding(window, device); }
} // namespace aurora::webgpu::utils
