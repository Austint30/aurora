#include "BackendBinding.hpp"

#include <SDL_video.h>
#include <dawn/native/OpenGLBackend.h>
#include <aurora/xr/openxr_platform.hpp>

namespace aurora::webgpu::utils {
class OpenGLBinding : public BackendBinding {
public:
  OpenGLBinding(SDL_Window* window, WGPUDevice device) : BackendBinding(window, device) {}

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
    return dawn::native::opengl::GetNativeSwapChainPreferredFormat(&m_swapChainImpl);
  }

  std::optional<XrStructureType> XrGetGraphicsBindingType() const override {
#ifdef XR_USE_PLATFORM_WIN32
    return XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
#elif defined(XR_USE_PLATFORM_XLIB)
    return XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
#elif defined(XR_USE_PLATFORM_XCB)
    return XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR;
#elif defined(XR_USE_PLATFORM_WAYLAND)
    return XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR;
#endif
    return std::nullopt;
  }
  std::vector<std::string> XrGetInstanceExtensions() const override { return {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME}; };

  void XrInitializeDevice() override {

  };

private:
  DawnSwapChainImplementation m_swapChainImpl{};

  void CreateSwapChainImpl() {
    m_swapChainImpl = dawn::native::opengl::CreateNativeSwapChainImpl(
        m_device, [](void* userdata) { SDL_GL_SwapWindow(static_cast<SDL_Window*>(userdata)); }, m_window);
  }
};

BackendBinding* CreateOpenGLBinding(SDL_Window* window, WGPUDevice device) { return new OpenGLBinding(window, device); }
} // namespace aurora::webgpu::utils
