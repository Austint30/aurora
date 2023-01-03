// Copyright 2018 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vector>

#include "dawn/dawn_wsi.h"
#include "dawn/webgpu_cpp.h"
#include "openxr/openxr.h"
#include "aurora/xr/openxr_platform.hpp"

namespace aurora::xr {

//using dawn::native::vulkan::Device;

class XrSwapChainImplVk {
public:
  using WSIContext = DawnWSIContextVulkan;
  XrSwapChainImplVk();
  ~XrSwapChainImplVk();

  void Init(DawnWSIContextVulkan* context);
  DawnSwapChainError Configure(WGPUTextureFormat format,
                               WGPUTextureUsage,
                               uint32_t width,
                               uint32_t height);
  DawnSwapChainError GetNextTexture(DawnSwapChainNextTexture* nextTexture);
  DawnSwapChainError Present();
  wgpu::TextureFormat GetPreferredFormat() const;
private:
//    void UpdateSurfaceConfig();
    XrSwapchain m_swapChain;
    std::vector<XrSwapchainImageVulkanKHR> m_swapChainImages;
    std::vector<XrSwapchainImageBaseHeader*> m_swapChainBaseHeaders;

//    Device* mDevice = nullptr;
};

} // aurora::xr
