#include "BackendBinding.hpp"

#include "../internal.hpp"
#include "../webgpu/gpu.hpp"
#include <aurora/xr/openxr_platform.hpp>
#include <aurora/xr/common.h>
#include <iostream>

#include <SDL_vulkan.h>
#include <dawn/native/VulkanBackend.h>

#define CHECK_VKCMD(cmd) CheckVkResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_VKRESULT(res, cmdStr) CheckVkResult(res, cmdStr, FILE_AND_LINE);

namespace aurora::webgpu::utils {
using aurora::webgpu::g_device;
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
  xr::aurora::xr::Throw(xr::Fmt("VkResult failure [%s]", vkResultString(res).c_str()), originator, sourceLocation);
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

  const char* GetValidationLayerName() {
    VkInstance vkInstance = dawn::native::vulkan::GetInstance(m_device);
    uint32_t layerCount;
    PFN_vkEnumerateInstanceLayerProperties pfnVkEnumerateInstanceLayerProperties =
        reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(dawn::native::vulkan::GetInstanceProcAddr(m_device, "vkEnumerateDeviceLayerProperties"));

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
  void XrInitializeDevice() override {

    VkInstance vkInstance = dawn::native::vulkan::GetInstance(m_device);

    VkDevice device = reinterpret_cast<VkDevice>(g_device.Get());

    PFN_vkEnumeratePhysicalDevices pfnVkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        dawn::native::vulkan::GetInstanceProcAddr(m_device, "vkEnumeratePhysicalDevices"));

//    pfnVkEnumeratePhysicalDevices();

    m_xrGraphicsBinding.instance = vkInstance;
    m_xrGraphicsBinding.device = device;

  }
  /*
    void XrInitializeDevice(XrInstance instance, XrSystemId systemId) override {

      VkInstance vkInstance = dawn::native::vulkan::GetInstance(m_device);

      // Create the Vulkan device for the adapter associated with the system.
      // Extension function must be loaded by name
      XrGraphicsRequirementsVulkan2KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
      PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetVulkanGraphicsRequirements2KHR = nullptr;
      CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirements2KHR)));
      pfnGetVulkanGraphicsRequirements2KHR(instance, systemId, &graphicsRequirements);

      VkResult err;

      // Get API validation layers (disabled when not in debug mode to improve performance)
      std::vector<const char*> layers;
  #if !defined(NDEBUG)
      const char* const validationLayerName = GetValidationLayerName();
      if (validationLayerName) {
        layers.push_back(validationLayerName);
      } else {
        Log.report(logvisor::Warning, FMT_STRING("No validation layers found in the system, skipping."));
      }
  #endif

      std::vector<const char*> extensions;
      extensions.push_back("VK_EXT_debug_report");
      VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
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
          reinterpret_cast<PFN_vkGetInstanceProcAddr>(&dawn::native::vulkan::GetInstanceProcAddr);
      createInfo.vulkanCreateInfo = &instInfo;
      createInfo.vulkanAllocator = nullptr;
      CHECK_XRCMD(XrCreateVulkanInstanceKHR(instance, &createInfo, &vkInstance, &err));
      CHECK_VKCMD(err);

    };

    // Note: The output must not outlive the input - this modifies the input and returns a collection of views into that modified
    // input!
    std::vector<const char*> ParseExtensionString(char* names) {
      std::vector<const char*> list;
      while (*names != 0) {
        list.push_back(names);
        while (*(++names) != 0) {
          if (*names == ' ') {
            *names++ = '\0';
            break;
          }
        }
      }
      return list;
    }

    virtual XrResult XrCreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo,
                                               VkInstance* vulkanInstance, VkResult* vulkanResult) {
      PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = nullptr;
      CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanInstanceExtensionsKHR)));

      uint32_t extensionNamesSize = 0;
      CHECK_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, 0, &extensionNamesSize, nullptr));

      std::vector<char> extensionNames(extensionNamesSize);
      CHECK_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, extensionNamesSize, &extensionNamesSize,
                                                    &extensionNames[0]));
      {
        // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
        std::vector<const char*> extensions = ParseExtensionString(&extensionNames[0]);

        // Merge the runtime's request with the applications requests
        for (uint32_t i = 0; i < createInfo->vulkanCreateInfo->enabledExtensionCount; ++i) {
          extensions.push_back(createInfo->vulkanCreateInfo->ppEnabledExtensionNames[i]);
        }

        VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        memcpy(&instInfo, createInfo->vulkanCreateInfo, sizeof(instInfo));
        instInfo.enabledExtensionCount = (uint32_t)extensions.size();
        instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

        auto pfnCreateInstance = (PFN_vkCreateInstance)createInfo->pfnGetInstanceProcAddr(nullptr, "vkCreateInstance");
        *vulkanResult = pfnCreateInstance(&instInfo, createInfo->vulkanAllocator, vulkanInstance);
      }

      return XR_SUCCESS;
    }
  */
private:
  DawnSwapChainImplementation m_swapChainImpl{};
  std::vector<VkPhysicalDevice> m_gpus;

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
