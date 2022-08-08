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

extern wgpu::Device g_device;
extern wgpu::Queue g_queue;
extern wgpu::SwapChain g_swapChain;
extern wgpu::BackendType g_backendType;
extern GraphicsConfig g_graphicsConfig;
extern TextureWithSampler g_frameBuffer;
extern TextureWithSampler g_frameBufferResolved;
extern TextureWithSampler g_depthBuffer;
extern wgpu::RenderPipeline g_CopyPipeline;
extern wgpu::BindGroup g_CopyBindGroup;
extern wgpu::Instance g_instance;

bool initialize(AuroraBackend backend);
void initialize_openxr(utils::BackendBinding& backendBinding);
void shutdown();
void resize_swapchain(uint32_t width, uint32_t height, bool force = false);
TextureWithSampler create_render_texture(bool multisampled);
} // namespace aurora::webgpu
