#pragma once

#include <dawn/native/DawnNative.h>
#include <webgpu/webgpu_cpp.h>
#include <openxr/openxr.h>
#include "aurora/xr/openxr_platform.hpp"
#include <optional>

struct SDL_Window;

namespace aurora::webgpu::utils {

class BackendBinding {
public:
  virtual ~BackendBinding() = default;

  virtual uint64_t GetSwapChainImplementation() = 0;
  virtual WGPUTextureFormat GetPreferredSwapChainTextureFormat() = 0;

  virtual std::optional<XrStructureType> XrGetGraphicsBindingType() const = 0;
  virtual std::vector<std::string> XrGetInstanceExtensions() const = 0;
  virtual void XrInitializeDevice() = 0;

  const XrBaseInStructure* GetGraphicsBinding() const {
    return reinterpret_cast<const XrBaseInStructure*>(&m_xrGraphicsBinding);
  }

protected:
  BackendBinding(SDL_Window* window, WGPUDevice device);

  SDL_Window* m_window = nullptr;
  WGPUDevice m_device = nullptr;
  XrGraphicsBindingVulkan2KHR m_xrGraphicsBinding;
};

bool DiscoverAdapter(dawn::native::Instance* instance, SDL_Window* window, wgpu::BackendType type);
BackendBinding* CreateBinding(wgpu::BackendType type, SDL_Window* window, WGPUDevice device);

} // namespace aurora::webgpu::utils
