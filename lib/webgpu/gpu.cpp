#include "gpu.hpp"

#include <aurora/aurora.h>

#include "../window.hpp"
#include "../internal.hpp"

#include <SDL.h>
#include <magic_enum.hpp>
#include <memory>
#include <algorithm>

#include "aurora/xr/xr.hpp"

#ifdef WEBGPU_DAWN
#include <dawn/native/DawnNative.h>
#include "../dawn/BackendBinding.hpp"
#endif

namespace aurora::webgpu {
static Module Log("aurora::gpu");

wgpu::Device g_device;
wgpu::Queue g_queue;
wgpu::BackendType g_backendType;

// Should be two elements for each eye in HMD plus one for mirror window, one if not using OpenXR.
std::vector<RenderView> g_renderViews;

std::vector<GraphicsConfig> g_graphicsConfigs;
GraphicsConfig g_primaryGraphicsConfig;
TextureWithSampler g_frameBuffer;
TextureWithSampler g_frameBufferResolved;
TextureWithSampler g_depthBuffer;

// EFB -> XFB copy pipeline
static wgpu::BindGroupLayout g_CopyBindGroupLayout;
wgpu::BindGroup g_CopyBindGroup;

#ifdef WEBGPU_DAWN
static std::unique_ptr<dawn::native::Instance> g_dawnInstance;
static dawn::native::Adapter g_adapter;
static std::vector<std::unique_ptr<utils::BackendBinding>> g_backendBinding;
#else
wgpu::Instance g_instance;
static wgpu::Adapter g_adapter;
#endif
static wgpu::Surface g_surface;
static wgpu::AdapterProperties g_adapterProperties;

bool can_render_imgui(RenderView renderView){
  switch (renderView.type) {
  case PRIMARY:
  case MIRROR:
    return true;
  default:
    return false;
  }
}

TextureWithSampler create_render_texture(GraphicsConfig graphicsConfig, bool multisampled) {
  const wgpu::Extent3D size{
      .width = graphicsConfig.swapChainDescriptor.width,
      .height = graphicsConfig.swapChainDescriptor.height,
      .depthOrArrayLayers = 1,
  };
  const auto format = graphicsConfig.swapChainDescriptor.format;
  uint32_t sampleCount = 1;
  if (multisampled) {
    sampleCount = graphicsConfig.msaaSamples;
  }
  const wgpu::TextureDescriptor textureDescriptor{
      .label = "Render texture",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
               wgpu::TextureUsage::CopyDst,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = format,
      .mipLevelCount = 1,
      .sampleCount = sampleCount,
  };
  auto texture = g_device.CreateTexture(&textureDescriptor);

  const wgpu::TextureViewDescriptor viewDescriptor{
      .label = "Render texture view",
      .dimension = wgpu::TextureViewDimension::e2D,
      .mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED,
      .arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED,
  };
  auto view = texture.CreateView(&viewDescriptor);

  const wgpu::SamplerDescriptor samplerDescriptor{
      .label = "Render sampler",
      .addressModeU = wgpu::AddressMode::ClampToEdge,
      .addressModeV = wgpu::AddressMode::ClampToEdge,
      .addressModeW = wgpu::AddressMode::ClampToEdge,
      .magFilter = wgpu::FilterMode::Linear,
      .minFilter = wgpu::FilterMode::Linear,
      .mipmapFilter = wgpu::FilterMode::Linear,
      .lodMinClamp = 0.f,
      .lodMaxClamp = 1000.f,
      .maxAnisotropy = 1,
  };
  auto sampler = g_device.CreateSampler(&samplerDescriptor);

  return {
      .texture = std::move(texture),
      .view = std::move(view),
      .size = size,
      .format = format,
      .sampler = std::move(sampler),
  };
}

static TextureWithSampler create_depth_texture(GraphicsConfig graphicsConfig) {
  const wgpu::Extent3D size{
      .width = graphicsConfig.swapChainDescriptor.width,
      .height = graphicsConfig.swapChainDescriptor.height,
      .depthOrArrayLayers = 1,
  };
  const auto format = graphicsConfig.depthFormat;
  const wgpu::TextureDescriptor textureDescriptor{
      .label = "Depth texture",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = format,
      .mipLevelCount = 1,
      .sampleCount = graphicsConfig.msaaSamples,
  };
  auto texture = g_device.CreateTexture(&textureDescriptor);

  const wgpu::TextureViewDescriptor viewDescriptor{
      .label = "Depth texture view",
      .dimension = wgpu::TextureViewDimension::e2D,
      .mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED,
      .arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED,
  };
  auto view = texture.CreateView(&viewDescriptor);

  const wgpu::SamplerDescriptor samplerDescriptor{
      .label = "Depth sampler",
      .addressModeU = wgpu::AddressMode::ClampToEdge,
      .addressModeV = wgpu::AddressMode::ClampToEdge,
      .addressModeW = wgpu::AddressMode::ClampToEdge,
      .magFilter = wgpu::FilterMode::Linear,
      .minFilter = wgpu::FilterMode::Linear,
      .mipmapFilter = wgpu::FilterMode::Linear,
      .lodMinClamp = 0.f,
      .lodMaxClamp = 1000.f,
      .maxAnisotropy = 1,
  };
  auto sampler = g_device.CreateSampler(&samplerDescriptor);

  return {
      .texture = std::move(texture),
      .view = std::move(view),
      .size = size,
      .format = format,
      .sampler = std::move(sampler),
  };
}

void create_copy_pipeline(RenderView& renderView) {
  wgpu::ShaderModuleWGSLDescriptor sourceDescriptor{};
  sourceDescriptor.source = R"""(
@group(0) @binding(0)
var efb_sampler: sampler;
@group(0) @binding(1)
var efb_texture: texture_2d<f32>;

struct VertexOutput {
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

var<private> pos: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(-1.0, 1.0),
    vec2(-1.0, -3.0),
    vec2(3.0, 1.0),
);
var<private> uvs: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(0.0, 0.0),
    vec2(0.0, 2.0),
    vec2(2.0, 0.0),
);

@vertex
fn vs_main(@builtin(vertex_index) vtxIdx: u32) -> VertexOutput {
    var out: VertexOutput;
    out.pos = vec4<f32>(pos[vtxIdx], 0.0, 1.0);
    out.uv = uvs[vtxIdx];
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return textureSample(efb_texture, efb_sampler, in.uv);
}
)""";
  const wgpu::ShaderModuleDescriptor moduleDescriptor{
      .nextInChain = &sourceDescriptor,
      .label = "XFB Copy Module",
  };
  auto module = g_device.CreateShaderModule(&moduleDescriptor);
  const std::array colorTargets{wgpu::ColorTargetState{
      .format = renderView.graphicsConfig.swapChainDescriptor.format,
      .writeMask = wgpu::ColorWriteMask::All,
  }};
  const wgpu::FragmentState fragmentState{
      .module = module,
      .entryPoint = "fs_main",
      .targetCount = colorTargets.size(),
      .targets = colorTargets.data(),
  };
  const std::array bindGroupLayoutEntries{
      wgpu::BindGroupLayoutEntry{
          .binding = 0,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler =
              wgpu::SamplerBindingLayout{
                  .type = wgpu::SamplerBindingType::Filtering,
              },
      },
      wgpu::BindGroupLayoutEntry{
          .binding = 1,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture =
              wgpu::TextureBindingLayout{
                  .sampleType = wgpu::TextureSampleType::Float,
                  .viewDimension = wgpu::TextureViewDimension::e2D,
              },
      },
  };
  const wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
      .entryCount = bindGroupLayoutEntries.size(),
      .entries = bindGroupLayoutEntries.data(),
  };
  g_CopyBindGroupLayout = g_device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);
  const wgpu::PipelineLayoutDescriptor layoutDescriptor{
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &g_CopyBindGroupLayout,
  };
  auto pipelineLayout = g_device.CreatePipelineLayout(&layoutDescriptor);
  const wgpu::RenderPipelineDescriptor pipelineDescriptor{
      .layout = pipelineLayout,
      .vertex =
          wgpu::VertexState{
              .module = module,
              .entryPoint = "vs_main",
          },
      .primitive =
          wgpu::PrimitiveState{
              .topology = wgpu::PrimitiveTopology::TriangleList,
          },
      .multisample =
          wgpu::MultisampleState{
              .count = 1,
              .mask = UINT32_MAX,
          },
      .fragment = &fragmentState,
  };
  renderView.copyPipeline = g_device.CreateRenderPipeline(&pipelineDescriptor);
}

void create_copy_bind_group(GraphicsConfig graphicsConfig) {
  const std::array bindGroupEntries{
      wgpu::BindGroupEntry{
          .binding = 0,
          .sampler = graphicsConfig.msaaSamples > 1 ? g_frameBufferResolved.sampler : g_frameBuffer.sampler,
      },
      wgpu::BindGroupEntry{
          .binding = 1,
          .textureView = graphicsConfig.msaaSamples > 1 ? g_frameBufferResolved.view : g_frameBuffer.view,
      },
  };
  const wgpu::BindGroupDescriptor bindGroupDescriptor{
      .layout = g_CopyBindGroupLayout,
      .entryCount = bindGroupEntries.size(),
      .entries = bindGroupEntries.data(),
  };
  g_CopyBindGroup = g_device.CreateBindGroup(&bindGroupDescriptor);
}

static void log_callback(WGPULoggingType type, char const * message, void * userdata) {
  AuroraLogLevel level = LOG_FATAL;
  switch (type) {
  case WGPULoggingType_Verbose:
    level = LOG_DEBUG;
    break;
  case WGPULoggingType_Info:
    level = LOG_INFO;
    break;
  case WGPULoggingType_Warning:
    level = LOG_WARNING;
    break;
  case WGPULoggingType_Error:
    level = LOG_ERROR;
    break;
  default:
    break;
  }
  Log.report(level, FMT_STRING("WebGPU message: {}"), message);
}

static void error_callback(WGPUErrorType type, char const* message, void* userdata) {
  FATAL("WebGPU error {}: {}", static_cast<int>(type), message);
}

#ifndef WEBGPU_DAWN
static void adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message,
                             void* userdata) {
  if (status == WGPURequestAdapterStatus_Success) {
    g_adapter = wgpu::Adapter::Acquire(adapter);
  } else {
    Log.report(LOG_WARNING, FMT_STRING("Adapter request failed with message: {}"), message);
  }
  *static_cast<bool*>(userdata) = true;
}
#endif

static void device_callback(WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
  if (status == WGPURequestDeviceStatus_Success) {
    g_device = wgpu::Device::Acquire(device);
  } else {
    Log.report(LOG_WARNING, FMT_STRING("Device request failed with message: {}"), message);
  }
  *static_cast<bool*>(userdata) = true;
}

static wgpu::BackendType to_wgpu_backend(AuroraBackend backend) {
  switch (backend) {
  case BACKEND_WEBGPU:
    return wgpu::BackendType::WebGPU;
  case BACKEND_D3D12:
    return wgpu::BackendType::D3D12;
  case BACKEND_METAL:
    return wgpu::BackendType::Metal;
  case BACKEND_VULKAN:
    return wgpu::BackendType::Vulkan;
  case BACKEND_OPENGL:
    return wgpu::BackendType::OpenGL;
  case BACKEND_OPENGLES:
    return wgpu::BackendType::OpenGLES;
  default:
    return wgpu::BackendType::Null;
  }
}

// Non VR (when useMirrorWindow is false)
void create_graphicsconfig(utils::BackendBinding& backendBinding, bool useMirrorWindow) {
#if WEBGPU_DAWN
  auto swapChainFormat = static_cast<wgpu::TextureFormat>(backendBinding.GetPreferredSwapChainTextureFormat());
#else
  auto swapChainFormat = g_surface.GetPreferredFormat(g_adapter);
#endif
  if (swapChainFormat == wgpu::TextureFormat::RGBA8UnormSrgb) {
    swapChainFormat = wgpu::TextureFormat::RGBA8Unorm;
  } else if (swapChainFormat == wgpu::TextureFormat::BGRA8UnormSrgb) {
    swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;
  }
  Log.report(LOG_INFO, FMT_STRING("Using swapchain format {}"), magic_enum::enum_name(swapChainFormat));
  const auto size = window::get_window_size();

  auto gc = GraphicsConfig {
      .swapChainDescriptor =
          wgpu::SwapChainDescriptor{
              .usage = wgpu::TextureUsage::RenderAttachment,
              .format = swapChainFormat,
              .width = size.fb_width,
              .height = size.fb_height,
              .presentMode = wgpu::PresentMode::Fifo,
#ifdef WEBGPU_DAWN
              .implementation = backendBinding.GetSwapChainImplementation(),
#endif
          },
      .depthFormat = wgpu::TextureFormat::Depth32Float, .msaaSamples = g_config.msaa,
      .textureAnisotropy = g_config.maxTextureAnisotropy,
  };

  g_graphicsConfigs.push_back(gc);
  RenderView renderView;
  RenderViewType renderViewType = useMirrorWindow ? RenderViewType::MIRROR : RenderViewType::PRIMARY;
  add_render_view(gc, renderViewType, renderView);

  create_copy_pipeline(g_renderViews.back());
  resize_swapchain(g_renderViews.back(), size.fb_width, size.fb_height, true);
}

// For VR
void create_xr_graphicsconfig(utils::BackendBinding& backendBinding) {
#if WEBGPU_DAWN
  auto swapChainFormat = static_cast<wgpu::TextureFormat>(backendBinding.GetPreferredSwapChainTextureFormat());
#else
  auto swapChainFormat = g_surface.GetPreferredFormat(g_adapter);
#endif

  Log.report(LOG_INFO, FMT_STRING("(OPENXR) Using swapchain format {}"), magic_enum::enum_name(swapChainFormat));

  std::vector<RenderViewType> types {OPENXR_LEFT, OPENXR_RIGHT};

  auto configViews = xr::g_OpenXRSessionManager->GetConfigViews();

  if (configViews.size() < 2) {
    Log.report(LOG_FATAL, FMT_STRING("OpenXR has less than two configviews!"));
  }

  for (int i = 0; i < 2; ++i) {
    RenderViewType renderViewType = types[i];
    XrViewConfigurationView view = configViews[i];
    auto gc = GraphicsConfig {
        .swapChainDescriptor =
            wgpu::SwapChainDescriptor{
                .usage = wgpu::TextureUsage::RenderAttachment,
                .format = swapChainFormat,
                .width = view.recommendedImageRectWidth,
                .height = view.recommendedImageRectHeight,
                .presentMode = wgpu::PresentMode::Fifo,
#ifdef WEBGPU_DAWN
                .implementation = backendBinding.GetSwapChainImplementation(),
#endif
            },
        .depthFormat = wgpu::TextureFormat::Depth32Float, .msaaSamples = g_config.msaa,
        .textureAnisotropy = g_config.maxTextureAnisotropy,
    };

    g_graphicsConfigs.push_back(gc);
    RenderView renderView;
    add_render_view(gc, renderViewType, renderView);

    create_copy_pipeline(g_renderViews.back());
    resize_swapchain(g_renderViews.back(), view.recommendedImageRectWidth, view.recommendedImageRectHeight, true);
  }
}

bool initialize(AuroraBackend auroraBackend) {
#ifdef WEBGPU_DAWN
  if (!g_dawnInstance) {
    Log.report(LOG_INFO, FMT_STRING("Creating Dawn instance"));
    g_dawnInstance = std::make_unique<dawn::native::Instance>();
  }
#else
  if (!g_instance) {
    const wgpu::InstanceDescriptor instanceDescriptor{};
    g_instance = {}; // TODO use wgpuCreateInstance when supported
  }
#endif
  wgpu::BackendType backend = to_wgpu_backend(auroraBackend);
#ifdef EMSCRIPTEN
  if (backend != wgpu::BackendType::WebGPU) {
    Log.report(LOG_WARNING, FMT_STRING("Backend type {} unsupported"), magic_enum::enum_name(backend));
    return false;
  }
#endif
  Log.report(LOG_INFO, FMT_STRING("Attempting to initialize {}"), magic_enum::enum_name(backend));
#if 0
  // D3D12's debug layer is very slow
  g_dawnInstance->EnableBackendValidation(backend != WGPUBackendType::D3D12);
#endif

#ifdef WEBGPU_DAWN
  SDL_Window* window = window::get_sdl_window();
  if (!utils::DiscoverAdapter(g_dawnInstance.get(), window, backend)) {
    return false;
  }
  {
    std::vector<dawn::native::Adapter> adapters = g_dawnInstance->GetAdapters();
    std::sort(adapters.begin(), adapters.end(), [&](const auto& a, const auto& b) {
      wgpu::AdapterProperties propertiesA;
      wgpu::AdapterProperties propertiesB;
      a.GetProperties(&propertiesA);
      b.GetProperties(&propertiesB);
      constexpr std::array PreferredTypeOrder{
          wgpu::AdapterType::DiscreteGPU,
          wgpu::AdapterType::IntegratedGPU,
          wgpu::AdapterType::CPU,
      };
      const auto typeItA = std::find(PreferredTypeOrder.begin(), PreferredTypeOrder.end(), propertiesA.adapterType);
      const auto typeItB = std::find(PreferredTypeOrder.begin(), PreferredTypeOrder.end(), propertiesB.adapterType);
      return typeItA < typeItB;
    });
    const auto adapterIt = std::find_if(adapters.begin(), adapters.end(), [=](const auto& adapter) -> bool {
      wgpu::AdapterProperties properties;
      adapter.GetProperties(&properties);
      return properties.backendType == backend;
    });
    if (adapterIt == adapters.end()) {
      return false;
    }
    g_adapter = *adapterIt;
  }
#else
  const WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDescriptor{
      .chain = {.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector},
      .selector = "#canvas",
  };
  const WGPUSurfaceDescriptor surfaceDescriptor{
      .nextInChain = &canvasDescriptor.chain,
      .label = "Surface",
  };
  g_surface = wgpu::Surface::Acquire(wgpuInstanceCreateSurface(g_instance.Get(), &surfaceDescriptor));
  ASSERT(g_surface, "Failed to initialize surface");
  const WGPURequestAdapterOptions options{
      .compatibleSurface = g_surface.Get(),
      .powerPreference = WGPUPowerPreference_HighPerformance,
      .forceFallbackAdapter = false,
  };
  bool adapterCallbackRecieved = false;
  wgpuInstanceRequestAdapter(g_instance.Get(), &options, adapter_callback, &adapterCallbackRecieved);
  while (!adapterCallbackRecieved) {
    emscripten_log(EM_LOG_CONSOLE, "Waiting for adapter...\n");
    emscripten_sleep(100);
  }
#endif
  g_adapter.GetProperties(&g_adapterProperties);
  g_backendType = g_adapterProperties.backendType;
  const auto backendName = magic_enum::enum_name(g_backendType);
  const char* adapterName = g_adapterProperties.name;
  if (adapterName == nullptr) {
    adapterName = "Unknown";
  }
  const char* driverDescription = g_adapterProperties.driverDescription;
  if (driverDescription == nullptr) {
    driverDescription = "Unknown";
  }
  Log.report(LOG_INFO, FMT_STRING("Graphics adapter information\n  API: {}\n  Device: {} ({})\n  Driver: {}"),
             backendName, adapterName, magic_enum::enum_name(g_adapterProperties.adapterType), driverDescription);

  {
// TODO: emscripten doesn't implement wgpuAdapterGetLimits
#ifdef WEBGPU_DAWN
    WGPUSupportedLimits supportedLimits{};
    g_adapter.GetLimits(&supportedLimits);
    const wgpu::RequiredLimits requiredLimits{
        .limits =
            {
                // Use "best" supported alignments
                .minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment == 0
                                                       ? static_cast<uint32_t>(WGPU_LIMIT_U32_UNDEFINED)
                                                       : supportedLimits.limits.minUniformBufferOffsetAlignment,
                .minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment == 0
                                                       ? static_cast<uint32_t>(WGPU_LIMIT_U32_UNDEFINED)
                                                       : supportedLimits.limits.minStorageBufferOffsetAlignment,
            },
    };
#endif
    std::vector<wgpu::FeatureName> features;
#ifdef WEBGPU_DAWN
    const auto supportedFeatures = g_adapter.GetSupportedFeatures();
    for (const auto* const feature : supportedFeatures) {
      if (strcmp(feature, "texture-compression-bc") == 0) {
        features.push_back(wgpu::FeatureName::TextureCompressionBC);
      }
    }
#else
    std::vector<wgpu::FeatureName> supportedFeatures;
    size_t featureCount = g_adapter.EnumerateFeatures(nullptr);
    supportedFeatures.resize(featureCount);
    g_adapter.EnumerateFeatures(supportedFeatures.data());
    for (const auto& feature : supportedFeatures) {
      if (feature == wgpu::FeatureName::TextureCompressionBC) {
        features.push_back(wgpu::FeatureName::TextureCompressionBC);
      }
    }
#endif
#ifdef WEBGPU_DAWN
    const std::array enableToggles {
      /* clang-format off */
#if _WIN32
      "use_dxc",
#endif
#ifdef NDEBUG
      "skip_validation",
      "disable_robustness",
#endif
      "use_user_defined_labels_in_backend",
      "disable_symbol_renaming",
      /* clang-format on */
    };
    wgpu::DawnTogglesDeviceDescriptor togglesDescriptor{};
    togglesDescriptor.forceEnabledTogglesCount = enableToggles.size();
    togglesDescriptor.forceEnabledToggles = enableToggles.data();
#endif
    const wgpu::DeviceDescriptor deviceDescriptor{
#ifdef WEBGPU_DAWN
        .nextInChain = &togglesDescriptor,
#endif
        .requiredFeaturesCount = static_cast<uint32_t>(features.size()),
        .requiredFeatures = features.data(),
#ifdef WEBGPU_DAWN
        .requiredLimits = &requiredLimits,
#endif
    };
    bool deviceCallbackReceived = false;
    g_adapter.RequestDevice(&deviceDescriptor, device_callback, &deviceCallbackReceived);
#ifdef EMSCRIPTEN
    while (!deviceCallbackReceived) {
      emscripten_log(EM_LOG_CONSOLE, "Waiting for device...\n");
      emscripten_sleep(100);
    }
#endif
    if (!g_device) {
      return false;
    }
    g_device.SetLoggingCallback(&log_callback, nullptr);
    g_device.SetUncapturedErrorCallback(&error_callback, nullptr);
  }
  g_device.SetDeviceLostCallback(nullptr, nullptr);
  g_queue = g_device.GetQueue();

  if (g_config.startOpenXR) {
    // Use OpenXR binding variant as main BackendBinding, and non-vr version as mirror binding.
    g_backendBinding.push_back(
        std::unique_ptr<utils::BackendBinding>(utils::CreateXrBinding(g_backendType, window, g_device.Get()))
    );
    if (g_backendBinding.size() == 0) {
      return false;
    }
  }

  g_backendBinding.push_back(
      std::unique_ptr<utils::BackendBinding>(utils::CreateBinding(g_backendType, window, g_device.Get()))
  );

  if (
      (g_backendBinding.size() == 1 && g_config.startOpenXR) ||
      (g_backendBinding.size() == 0)
      ) {
    return false;
  }

  if (g_config.startOpenXR && g_backendBinding[0]->GetXrGraphicsBinding()) {
    XrBaseInStructure* graphicsBinding = (XrBaseInStructure*)(g_backendBinding[0]->GetXrGraphicsBinding());
    xr::g_OpenXRSessionManager->initializeSession(*graphicsBinding);
  }

  if (g_config.startOpenXR) {
    create_xr_graphicsconfig(*g_backendBinding[0]);
    g_primaryGraphicsConfig = g_graphicsConfigs[0];
    create_graphicsconfig(*g_backendBinding[1], true);
  }
  else {
    create_graphicsconfig(*g_backendBinding[0], false);
    g_primaryGraphicsConfig = g_graphicsConfigs[0];
  }

  return true;
}

/*
void initialize_openxr(utils::BackendBinding& backendBinding){
  xr::OpenXROptions options;
  std::shared_ptr<xr::OpenXRSessionManager> sesMgr = xr::InstantiateOXRSessionManager(options);

  sesMgr->createInstance(backendBinding);
  sesMgr->initializeSystem();
  sesMgr->initializeSession(backendBinding);
}
 */

void shutdown() {
  g_CopyBindGroupLayout = {};
  g_CopyBindGroup = {};
  g_frameBuffer = {};
  g_frameBufferResolved = {};
  g_depthBuffer = {};
  for (auto& renderView : g_renderViews) {
    wgpuSwapChainRelease(renderView.swapChain.Release());
    renderView.copyPipeline = {};
  };
  wgpuQueueRelease(g_queue.Release());
  wgpuDeviceDestroy(g_device.Release());
  g_adapter = {};
#ifdef WEBGPU_DAWN
  for (auto& backendBinding : g_backendBinding) {
    backendBinding.reset();
  }
  g_dawnInstance.reset();
#else
  g_surface = {};
  g_instance = {};
#endif
}

bool add_render_view(GraphicsConfig graphicsConfig, RenderViewType renderViewType, RenderView& newSwapChain) {
  RenderView ctx;
  ctx.swapChain = g_device.CreateSwapChain(g_surface, &graphicsConfig.swapChainDescriptor);
  ctx.type = renderViewType;
  ctx.graphicsConfig = graphicsConfig;

  if (std::find(g_renderViews.begin(), g_renderViews.end(), ctx) != g_renderViews.end()){
    // Swap chain already exists!
    Log.report(LOG_ERROR, FMT_STRING("Created swapchain that already exists!"));
    return false;
  }
  g_renderViews.push_back(ctx);
  newSwapChain = ctx;
  return true;
}

void resize_swapchain(RenderView& renderView, uint32_t width, uint32_t height, bool force) {

  if (!force && renderView.graphicsConfig.swapChainDescriptor.width == width &&
      renderView.graphicsConfig.swapChainDescriptor.height == height) {
    return;
  }
  renderView.graphicsConfig.swapChainDescriptor.width = width;
  renderView.graphicsConfig.swapChainDescriptor.height = height;

#ifdef WEBGPU_DAWN
  renderView.swapChain.Configure(renderView.graphicsConfig.swapChainDescriptor.format, renderView.graphicsConfig.swapChainDescriptor.usage, width,
                        height);
#else
  g_swapChain = g_device.CreateSwapChain(g_surface, &g_graphicsConfig.swapChainDescriptor);
#endif
  g_frameBuffer = create_render_texture(renderView.graphicsConfig, true);
  g_frameBufferResolved = create_render_texture(renderView.graphicsConfig, false);
  g_depthBuffer = create_depth_texture(renderView.graphicsConfig);
  create_copy_bind_group(renderView.graphicsConfig);
}
} // namespace aurora::webgpu
