#pragma once

#include <aurora/aurora.h>
#include <openxr/openxr.h>

#include "wgpu.hpp"
#include "../dawn/BackendBinding.hpp"

#include <array>
#include <cstdint>

struct SDL_Window;

namespace aurora::webgpu {
struct GraphicsConfig {
  wgpu::SwapChainDescriptor swapChainDescriptor;
  wgpu::TextureFormat depthFormat;
  uint32_t msaaSamples;
  uint16_t textureAnisotropy;
};
struct TextureWithSampler {
  wgpu::Texture texture;
  wgpu::TextureView view;
  wgpu::Extent3D size;
  wgpu::TextureFormat format;
  wgpu::Sampler sampler;
};

enum RenderViewType {
  PRIMARY, // For non-vr mode
  MIRROR, // For VR mirror window
  OPENXR_LEFT, // Left eye
  OPENXR_RIGHT // Right eye
};

struct RenderView {
  wgpu::SwapChain swapChain;
  GraphicsConfig graphicsConfig;
  wgpu::RenderPipeline copyPipeline;
  RenderViewType type;

  bool operator==(const RenderView& st) {
      return type == st.type;
  };
};

bool can_render_imgui(RenderView renderView);

extern wgpu::Device g_device;
extern wgpu::Queue g_queue;
extern std::vector<RenderView> g_renderViews;
extern wgpu::BackendType g_backendType;
extern std::vector<GraphicsConfig> g_graphicsConfigs;
extern GraphicsConfig g_primaryGraphicsConfig;
extern TextureWithSampler g_frameBuffer;
extern TextureWithSampler g_frameBufferResolved;
extern TextureWithSampler g_depthBuffer;
extern wgpu::BindGroup g_CopyBindGroup;
extern wgpu::Instance g_instance;

bool initialize(AuroraBackend backend);
void create_graphicsconfig();
void shutdown();
bool add_render_view(GraphicsConfig graphicsConfig, RenderViewType renderViewType, RenderView& newSwapChain);
void resize_swapchain(RenderView& renderView, uint32_t width, uint32_t height, bool force);
TextureWithSampler create_render_texture(bool multisampled);
} // namespace aurora::webgpu
