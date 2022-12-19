#include "BackendBinding.hpp"
#include <openxr/openxr.h>
#include "aurora/xr/openxr_platform.hpp"
#include "../xr/xr.hpp"
#include "../xr/check.h"
#include "../webgpu/gpu.hpp"

#if defined(DAWN_ENABLE_BACKEND_D3D12)
#include <dawn/native/D3D12Backend.h>
#endif
#if defined(DAWN_ENABLE_BACKEND_METAL)
#include <dawn/native/MetalBackend.h>
#endif
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
#include <dawn/native/VulkanBackend.h>
#endif
#if defined(DAWN_ENABLE_BACKEND_OPENGL)
#include <SDL_video.h>
#include <dawn/native/OpenGLBackend.h>
#endif
#if defined(DAWN_ENABLE_BACKEND_NULL)
#include <dawn/native/NullBackend.h>
#endif

namespace aurora::webgpu::utils {

#if defined(DAWN_ENABLE_BACKEND_D3D12)
BackendBinding* CreateD3D12Binding(SDL_Window* window, WGPUDevice device);
#endif
#if defined(DAWN_ENABLE_BACKEND_METAL)
BackendBinding* CreateMetalBinding(SDL_Window* window, WGPUDevice device);
#endif
#if defined(DAWN_ENABLE_BACKEND_NULL)
BackendBinding* CreateNullBinding(SDL_Window* window, WGPUDevice device);
#endif
#if defined(DAWN_ENABLE_BACKEND_OPENGL)
BackendBinding* CreateOpenGLBinding(SDL_Window* window, WGPUDevice device);
#endif
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
BackendBinding* CreateVulkanBinding(SDL_Window* window, WGPUDevice device);
#endif

BackendBinding::BackendBinding(SDL_Window* window, WGPUDevice device) : m_window(window), m_device(device) {}

#if defined(DAWN_ENABLE_BACKEND_OPENGL)
struct GLUserData {
  SDL_Window* window;
  SDL_GLContext context;
};
void GLMakeCurrent(void* userData) {
  auto* data = static_cast<GLUserData*>(userData);
  SDL_GL_MakeCurrent(data->window, data->context);
}
void GLDestroy(void* userData) {
  auto* data = static_cast<GLUserData*>(userData);
  SDL_GL_DeleteContext(data->context);
  delete data;
}
#endif

const WGPUDevice BackendBinding::GetDevice() { return m_device; }

bool DiscoverAdapter(dawn::native::Instance* instance, SDL_Window* window, wgpu::BackendType type) {
  switch (type) {
#if defined(DAWN_ENABLE_BACKEND_D3D12)
  case wgpu::BackendType::D3D12: {
    dawn::native::d3d12::AdapterDiscoveryOptions options;
    return instance->DiscoverAdapters(&options);
  }
#endif
#if defined(DAWN_ENABLE_BACKEND_METAL)
  case wgpu::BackendType::Metal: {
    dawn::native::metal::AdapterDiscoveryOptions options;
    return instance->DiscoverAdapters(&options);
  }
#endif
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
  case wgpu::BackendType::Vulkan: {
    if (xr::g_OpenXRSessionManager != nullptr){
      auto *vkCreateInstanceCallback = +[](const VkInstanceCreateInfo* vkCreateInfo, const VkAllocationCallbacks* allocator, VkInstance* vkInstance, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr) {
        XrInstance xrInstance = xr::g_OpenXRSessionManager->GetInstance();
        XrSystemId xrSystemId = xr::g_OpenXRSessionManager->GetSystemId();

        PFN_xrCreateVulkanInstanceKHR createVulkanInstancePFN = nullptr;
        xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanInstanceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&createVulkanInstancePFN));

        VkResult vkResult;

        XrVulkanInstanceCreateInfoKHR createInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        createInfo.systemId = xrSystemId;
        createInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        createInfo.vulkanCreateInfo = vkCreateInfo;
        createInfo.vulkanAllocator = allocator;

        xr::CHECK_XRCMD(createVulkanInstancePFN(xrInstance, &createInfo, vkInstance, &vkResult));
        xr::Log.report(LOG_INFO, FMT_STRING("xrCreateVulkanInstance successful"));
        return vkResult;
      };
      auto *gatherPhysicalDevicesCallback = +[](VkInstance instance, const PFN_vkGetInstanceProcAddr& vkGetInstanceProcAddr) {
        XrInstance xrInstance = xr::g_OpenXRSessionManager->GetInstance();
        XrSystemId xrSystemId = xr::g_OpenXRSessionManager->GetSystemId();

        std::vector<VkPhysicalDevice> physicalDevices(1);

        PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2PFN = nullptr;
        xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsDevice2PFN));

        XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        deviceGetInfo.systemId = xrSystemId;
        deviceGetInfo.vulkanInstance = instance;

        xr::CHECK_XRCMD(xrGetVulkanGraphicsDevice2PFN(xrInstance, &deviceGetInfo, &physicalDevices[0]));

        xr::Log.report(LOG_INFO, FMT_STRING("xrGetVulkanGraphicsDevice2KHR successful"));

        return std::move(physicalDevices);
      };
      auto *vkCreateDevice = +[](VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* deviceCreateInfo, const VkAllocationCallbacks* allocator, VkDevice* vkDevice, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr){
        XrInstance xrInstance = xr::g_OpenXRSessionManager->GetInstance();
        XrSystemId xrSystemId = xr::g_OpenXRSessionManager->GetSystemId();


        // Call xrGetVulkanGraphicsRequirements2KHR because we have to for some reason ---------------------------------
        XrGraphicsRequirementsVulkan2KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2PFN = nullptr;
        xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirements2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsRequirements2PFN));

        xrGetVulkanGraphicsRequirements2PFN(xrInstance, xrSystemId, &graphicsRequirements);
        // -------------------------------------------------------------------------------------------------------------

        PFN_xrCreateVulkanDeviceKHR createVulkanDevicePFN = nullptr;
        xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanDeviceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&createVulkanDevicePFN));

        VkResult vkResult;

        XrVulkanDeviceCreateInfoKHR createInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        createInfo.systemId = xrSystemId;
        createInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        createInfo.vulkanCreateInfo = deviceCreateInfo;
        createInfo.vulkanPhysicalDevice = physicalDevice;
        createInfo.vulkanAllocator = allocator;

        xr::CHECK_XRCMD(createVulkanDevicePFN(xrInstance, &createInfo, vkDevice, &vkResult))

        xr::Log.report(LOG_INFO, FMT_STRING("xrCreateVulkanDeviceKHR successful"));

        return vkResult;
      };
      dawn::native::vulkan::OverrideFunctions overrideFunctions = {
          vkCreateInstanceCallback, gatherPhysicalDevicesCallback,
          vkCreateDevice
      };

      auto options = dawn::native::vulkan::AdapterDiscoveryOptions(overrideFunctions);
      return instance->DiscoverAdapters(&options);
    };
    dawn::native::vulkan::AdapterDiscoveryOptions options;
    return instance->DiscoverAdapters(&options);
  }
#endif
#if defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
  case wgpu::BackendType::OpenGL: {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    dawn::native::opengl::AdapterDiscoveryOptions adapterOptions{WGPUBackendType_OpenGL};
    adapterOptions.getProc = SDL_GL_GetProcAddress;
    adapterOptions.makeCurrent = GLMakeCurrent;
    adapterOptions.destroy = GLDestroy;
    adapterOptions.userData = new GLUserData{
        .window = window,
        .context = context,
    };
    return instance->DiscoverAdapters(&adapterOptions);
  }
#endif
#if defined(DAWN_ENABLE_BACKEND_OPENGLES)
  case wgpu::BackendType::OpenGLES: {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    dawn::native::opengl::AdapterDiscoveryOptions adapterOptions{WGPUBackendType_OpenGLES};
    adapterOptions.getProc = SDL_GL_GetProcAddress;
    adapterOptions.makeCurrent = GLMakeCurrent;
    adapterOptions.destroy = GLDestroy;
    adapterOptions.userData = new GLUserData{
        .window = window,
        .context = context,
    };
    return instance->DiscoverAdapters(&adapterOptions);
  }
#endif
#if defined(DAWN_ENABLE_BACKEND_NULL)
  case wgpu::BackendType::Null:
    instance->DiscoverDefaultAdapters();
    return true;
#endif
  default:
    return false;
  }
}

BackendBinding* CreateBinding(wgpu::BackendType type, SDL_Window* window, WGPUDevice device) {
  switch (type) {
#if defined(DAWN_ENABLE_BACKEND_D3D12)
  case wgpu::BackendType::D3D12:
    return CreateD3D12Binding(window, device);
#endif
#if defined(DAWN_ENABLE_BACKEND_METAL)
  case wgpu::BackendType::Metal:
    return CreateMetalBinding(window, device);
#endif
#if defined(DAWN_ENABLE_BACKEND_NULL)
  case wgpu::BackendType::Null:
    return CreateNullBinding(window, device);
#endif
#if defined(DAWN_ENABLE_BACKEND_DESKTOP_GL)
  case wgpu::BackendType::OpenGL:
    return CreateOpenGLBinding(window, device);
#endif
#if defined(DAWN_ENABLE_BACKEND_OPENGLES)
  case wgpu::BackendType::OpenGLES:
    return CreateOpenGLBinding(window, device);
#endif
#if defined(DAWN_ENABLE_BACKEND_VULKAN)
  case wgpu::BackendType::Vulkan:
    return CreateVulkanBinding(window, device);
#endif
  default:
    return nullptr;
  }
}

} // namespace aurora::webgpu::utils
