#include <aurora/aurora.h>

#include "gfx/common.hpp"
#include "imgui.hpp"
#include "internal.hpp"
#include "webgpu/gpu.hpp"
#include "window.hpp"
#include "aurora/xr/xr.hpp"

#include <SDL_filesystem.h>
#include <imgui.h>

namespace aurora {
static Module Log("aurora");

AuroraConfig g_config;
int g_CurrentXrView;

// GPU
using webgpu::g_device;
using webgpu::g_queue;
using webgpu::g_renderViews;
static int g_currSwapChainIndex = -1;

constexpr std::array PreferredBackendOrder{
#ifdef ENABLE_BACKEND_WEBGPU
    BACKEND_WEBGPU,
#endif
#ifdef DAWN_ENABLE_BACKEND_D3D12
//    BACKEND_D3D12,
#endif
#ifdef DAWN_ENABLE_BACKEND_METAL
    BACKEND_METAL,
#endif
#ifdef DAWN_ENABLE_BACKEND_VULKAN
    BACKEND_VULKAN,
#endif
#ifdef DAWN_ENABLE_BACKEND_DESKTOP_GL
    BACKEND_OPENGL,
#endif
#ifdef DAWN_ENABLE_BACKEND_OPENGLES
    BACKEND_OPENGLES,
#endif
#ifdef DAWN_ENABLE_BACKEND_NULL
    BACKEND_NULL,
#endif
};

static bool g_initialFrame = false;

static std::vector<std::string> aurora_backend_to_xr_ext(AuroraBackend backend){
  switch (backend) {
  case BACKEND_WEBGPU: return {};
#ifdef XR_USE_GRAPHICS_API_D3D12
  case BACKEND_D3D12: return {XR_KHR_D3D12_ENABLE_EXTENSION_NAME};
#endif
#ifdef XR_USE_GRAPHICS_API_METAL
  case BACKEND_METAL: return {}; // METAL NOT IMPLEMENTED YET
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
  case BACKEND_VULKAN: return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL
  case BACKEND_OPENGL: return {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
  case BACKEND_OPENGLES: return {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
#endif
  default:
    return {};
  }
}

static AuroraInfo initialize(int argc, char* argv[], const AuroraConfig& config) noexcept {
  g_config = config;
  if (g_config.appName == nullptr) {
    g_config.appName = "Aurora";
  }
  if (g_config.configPath == nullptr) {
    g_config.configPath = SDL_GetPrefPath(nullptr, g_config.appName);
  }
  if (g_config.msaa == 0) {
    g_config.msaa = 1;
  }
  if (g_config.maxTextureAnisotropy == 0) {
    g_config.maxTextureAnisotropy = 16;
  }

  window::initialize();

  /* Attempt to create a window using the calling application's desired backend */
  AuroraBackend selectedBackend = config.desiredBackend;

  if (g_config.startOpenXR) {
    Log.report(LOG_INFO, FMT_STRING("Enabling OpenXR support"));
    xr::OpenXROptions options;
    // Create session manager instance
    std::shared_ptr<xr::OpenXRSessionManager> sesMgr = xr::InstantiateOXRSessionManager(options);

    sesMgr->createInstance(aurora_backend_to_xr_ext(selectedBackend));
    sesMgr->initializeSystem();
  }

  bool windowCreated = false;
  if (selectedBackend != BACKEND_AUTO && window::create_window(selectedBackend)) {
    if (webgpu::initialize(selectedBackend)) {
      windowCreated = true;
    } else {
      window::destroy_window();
    }
  }

  if (!windowCreated) {
    for (const auto backendType : PreferredBackendOrder) {
      selectedBackend = backendType;
      if (!window::create_window(selectedBackend)) {
        continue;
      }
      if (webgpu::initialize(selectedBackend)) {
        windowCreated = true;
        break;
      } else {
        window::destroy_window();
      }
    }
  }

  ASSERT(windowCreated, "Error creating window: {}", SDL_GetError());

  // Initialize SDL_Renderer for ImGui when we can't use a Dawn backend
  if (webgpu::g_backendType == wgpu::BackendType::Null) {
    ASSERT(window::create_renderer(), "Failed to initialize SDL renderer: {}", SDL_GetError());
  }

  window::show_window();
  gfx::initialize();

  imgui::create_context();
  const auto size = window::get_window_size();
  Log.report(LOG_INFO, FMT_STRING("Using framebuffer size {}x{} scale {}"), size.fb_width, size.fb_height, size.scale);
  if (g_config.imGuiInitCallback != nullptr) {
    g_config.imGuiInitCallback(&size);
  }
  imgui::initialize();

  aurora_begin_frame();
  if (aurora_begin_render_view()) {
    g_initialFrame = true;
  }
  g_config.desiredBackend = selectedBackend;
  return {
      .backend = selectedBackend,
      .configPath = g_config.configPath,
      .window = window::get_sdl_window(),
      .windowSize = size,
  };
}

#ifndef EMSCRIPTEN
static wgpu::TextureView g_currentView;
#endif

static void shutdown() noexcept {
#ifndef EMSCRIPTEN
  g_currentView = {};
#endif
  imgui::shutdown();
  gfx::shutdown();
  webgpu::shutdown();
  window::shutdown();
}

static const AuroraEvent* update() noexcept {
  if (g_initialFrame) {
    aurora_end_frame();
    g_initialFrame = false;
  }
  const auto* events = window::poll_events();
  imgui::new_frame(window::get_window_size());
  return events;
}

static bool begin_render_view(){

  webgpu::RenderView renderView = g_renderViews[g_currSwapChainIndex];

  if (renderView.type == webgpu::OPENXR_LEFT){
    g_CurrentXrView = 0;
  }
  else if (renderView.type == webgpu::OPENXR_RIGHT){
    aurora::g_CurrentXrView = 1;
  }

#ifndef EMSCRIPTEN
  g_currentView = renderView.swapChain.GetCurrentTextureView();
  if (!g_currentView) {
    ImGui::EndFrame();
    // Force swapchain recreation
    const auto size = window::get_window_size();
    int32_t width = renderView.graphicsConfig.swapChainDescriptor.width;
    int32_t height = renderView.graphicsConfig.swapChainDescriptor.height;
    webgpu::resize_swapchain(renderView, width, height, true);
    return false;
  }
#endif

  gfx::begin_frame();
  return true;
}

static void end_render_view(){
  webgpu::RenderView renderView = g_renderViews[g_currSwapChainIndex];
  g_currSwapChainIndex++;
  const auto encoderDescriptor = wgpu::CommandEncoderDescriptor{
      .label = "Redraw encoder",
  };
  auto encoder = g_device.CreateCommandEncoder(&encoderDescriptor);
  gfx::end_frame(encoder);
  gfx::render(encoder); // Change to reuse prev render on mirror swapchain
  {
    const std::array attachments{
        wgpu::RenderPassColorAttachment{
#ifdef EMSCRIPTEN
            .view = g_swapChain.GetCurrentTextureView(),
#else
            .view = g_currentView,
#endif
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
        },
    };
    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .label = "Post render pass",
        .colorAttachmentCount = attachments.size(),
        .colorAttachments = attachments.data(),
    };
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    // Copy EFB -> XFB (swapchain)
    pass.SetPipeline(renderView.copyPipeline);
    pass.SetBindGroup(0, webgpu::g_CopyBindGroup, 0, nullptr);
    pass.Draw(3);
    if (!g_initialFrame && (renderView.type == webgpu::PRIMARY || renderView.type == webgpu::MIRROR)) {
      // Render ImGui
      imgui::render(pass);
    }
    pass.End();
  }
  const wgpu::CommandBufferDescriptor cmdBufDescriptor{.label = "Redraw command buffer"};
  const auto buffer = encoder.Finish(&cmdBufDescriptor);
  g_queue.Submit(1, &buffer);
#ifdef WEBGPU_DAWN
  renderView.swapChain.Present();
  g_currentView = {};
#else
  emscripten_sleep(0);
#endif
}

static void begin_frame() noexcept {
  g_currSwapChainIndex = 0;
}

static void end_frame() noexcept {
  g_currSwapChainIndex = -1; // Probably not needed
}

static bool has_render_view(){
  return g_currSwapChainIndex < g_renderViews.size();
}

} // namespace aurora

// C API bindings
AuroraInfo aurora_initialize(int argc, char* argv[], const AuroraConfig* config) {
  return aurora::initialize(argc, argv, *config);
}
void aurora_shutdown() { aurora::shutdown(); }
const AuroraEvent* aurora_update() { return aurora::update(); }
void aurora_begin_frame() { aurora::begin_frame(); }
void aurora_end_frame() { aurora::end_frame(); }
bool aurora_begin_render_view(){ return aurora::begin_render_view(); };
void aurora_end_render_view(){ aurora::end_render_view(); }
bool aurora_has_render_view(){ return aurora::has_render_view(); };

AuroraBackend aurora_get_backend() { return aurora::g_config.desiredBackend; }
const AuroraBackend* aurora_get_available_backends(size_t* count) {
  *count = aurora::PreferredBackendOrder.size();
  return aurora::PreferredBackendOrder.data();
}
