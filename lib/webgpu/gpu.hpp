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

enum SwapChainType {
  PRIMARY, // For non-vr mode
  MIRROR, // For VR mirror window
  OPENXR_LEFT, // Left eye
  OPENXR_RIGHT // Right eye
};

struct SwapChainContext {
  wgpu::SwapChain swapChain;
  GraphicsConfig graphicsConfig;
  SwapChainType type;

  bool operator==(const SwapChainContext& st) {
      return type == st.type;
  };
};

bool can_render_imgui(SwapChainContext swapChainCtx){
  switch (swapChainCtx.type) {
  case PRIMARY:
  case MIRROR:
      return true;
  default:
      return false;
  }
}

extern wgpu::Device g_device;
extern wgpu::Queue g_queue;
extern std::vector<SwapChainContext> g_swapChains;
extern wgpu::BackendType g_backendType;
extern std::vector<GraphicsConfig> g_graphicsConfigs;
extern TextureWithSampler g_frameBuffer;
extern TextureWithSampler g_frameBufferResolved;
extern TextureWithSampler g_depthBuffer;
extern wgpu::RenderPipeline g_CopyPipeline;
extern wgpu::BindGroup g_CopyBindGroup;
extern wgpu::Instance g_instance;

bool initialize(AuroraBackend backend);
void create_graphicsconfig();
void shutdown();
bool add_swapchain(GraphicsConfig graphicsConfig, SwapChainType swapChainType, SwapChainContext& newSwapChain);
void resize_swapchain(SwapChainContext swapChainCtx, uint32_t width, uint32_t height, bool force);
TextureWithSampler create_render_texture(bool multisampled);
void create_graphicsconfig(utils::BackendBinding& backendBinding, bool useMirrorWindow);
void create_xr_graphicsconfig(utils::BackendBinding& backendBinding);
} // namespace aurora::webgpu
