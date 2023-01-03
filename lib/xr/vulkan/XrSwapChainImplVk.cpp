// Copyright 2018 The Dawn Authors & Austin Thibodeaux
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

#include "XrSwapChainImplVk.h"

#include <limits>
//#include "dawn/native/vulkan/DeviceVk.h"
//#include "dawn/native/vulkan/FencedDeleter.h"
//#include "dawn/native/vulkan/TextureVk.h"
#include "dawn/webgpu_cpp.h"
#include "aurora/xr/xr.hpp"
#include "../check.h"

namespace aurora::xr {

using aurora::g_CurrentXrView;

int64_t SelectColorSwapchainFormat(const std::vector<int64_t>& runtimeFormats){
  // List of supported color swapchain formats.
  constexpr int64_t SupportedColorSwapchainFormats[] = {VK_FORMAT_B8G8R8A8_UNORM,
                                                        VK_FORMAT_R8G8B8A8_UNORM,
                                                        VK_FORMAT_B8G8R8A8_SRGB,
                                                        VK_FORMAT_R8G8B8A8_SRGB};

  auto swapchainFormatIt =
      std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), std::begin(SupportedColorSwapchainFormats),
                         std::end(SupportedColorSwapchainFormats));
  if (swapchainFormatIt == runtimeFormats.end()) {
    THROW("No runtime swapchain format supported for color swapchain");
  }

  return *swapchainFormatIt;
}

wgpu::TextureFormat ConvertToWgpuTextureFormat(int64_t vulkanFormat){
  switch (vulkanFormat) {
  case VK_FORMAT_B8G8R8A8_UNORM:
    return wgpu::TextureFormat::BGRA8Unorm;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return wgpu::TextureFormat::RGBA8Unorm;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return wgpu::TextureFormat::BGRA8UnormSrgb;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return wgpu::TextureFormat::RGBA8UnormSrgb;
  }
}

// Converts the Dawn usage flags to Vulkan usage flags. Also needs the format to choose
// between color and depth attachment usages.
XrSwapchainUsageFlags OpenXRImageUsage(wgpu::TextureUsage usage) {
  XrSwapchainUsageFlags flags = 0;

  if (usage & wgpu::TextureUsage::CopySrc) {
    flags |= XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
  }
  if (usage & wgpu::TextureUsage::CopyDst) {
    flags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
  }
  if (usage & wgpu::TextureUsage::TextureBinding) {
    flags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    // If the sampled texture is a depth/stencil texture, its image layout will be set
    // to DEPTH_STENCIL_READ_ONLY_OPTIMAL in order to support readonly depth/stencil
    // attachment. That layout requires DEPTH_STENCIL_ATTACHMENT_BIT image usage.
    /*
    if (format.HasDepthOrStencil() && format.isRenderable) {
      flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
     */
  }
//  if (usage & wgpu::TextureUsage::StorageBinding) {
//    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
//  }
  if (usage & wgpu::TextureUsage::RenderAttachment) {
    /*
    if (format.HasDepthOrStencil()) {
      flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else {
     */
      flags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    //}
  }
//  flags |= XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  return flags;
}

//namespace {
//bool chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes,
//                           bool turnOffVsync,
//                           VkPresentModeKHR* presentMode) {
//    if (turnOffVsync) {
//        for (const auto& availablePresentMode : availablePresentModes) {
//            if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
//                *presentMode = availablePresentMode;
//                return true;
//            }
//        }
//        return false;
//    }
//
//    *presentMode = VK_PRESENT_MODE_FIFO_KHR;
//    return true;
//}
//
//bool ChooseSurfaceConfig(const dawn::native::vulkan::VulkanSurfaceInfo& info, XrSwapChainImplVk::ChosenConfig* config,
//                         bool turnOffVsync) {
//    VkPresentModeKHR presentMode;
//    if (!chooseSwapPresentMode(info.presentModes, turnOffVsync, &presentMode)) {
//        return false;
//    }
//    // TODO(crbug.com/dawn/269): For now this is hardcoded to what works with one NVIDIA
//    // driver. Need to generalize
//    config->nativeFormat = VK_FORMAT_B8G8R8A8_UNORM;
//    config->colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
//    config->format = wgpu::TextureFormat::BGRA8Unorm;
//    config->minImageCount = 3;
//    // TODO(crbug.com/dawn/269): This is upside down compared to what we want, at least
//    // on Linux
//    config->preTransform = info.capabilities.currentTransform;
//    config->presentMode = presentMode;
//    config->compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
//
//    return true;
//}
//}  // anonymous namespace

XrSwapChainImplVk::XrSwapChainImplVk() {
    // Call this immediately, so that BackendBinding::GetPreferredSwapChainTextureFormat
    // will return a correct result before a SwapChain is created.
//    UpdateSurfaceConfig();
}

XrSwapChainImplVk::~XrSwapChainImplVk() {
//    if (mSwapChain != VK_NULL_HANDLE) {
//        mDevice->GetFencedDeleter()->DeleteWhenUnused(mSwapChain);
//        mSwapChain = VK_NULL_HANDLE;
//    }
//    if (mSurface != VK_NULL_HANDLE) {
//        mDevice->GetFencedDeleter()->DeleteWhenUnused(mSurface);
//        mSurface = VK_NULL_HANDLE;
//    }
}

//void XrSwapChainImplVk::UpdateSurfaceConfig() {
////    if (mDevice->ConsumedError(GatherSurfaceInfo(*ToBackend(mDevice->GetAdapter()), mSurface),
////                               &mInfo)) {
////        ASSERT(false, "");
////    }
////
////    if (!ChooseSurfaceConfig(mInfo, &mConfig, mDevice->IsToggleEnabled(Toggle::TurnOffVsync))) {
////        ASSERT(false, "");
////    }
//}

void XrSwapChainImplVk::Init(DawnWSIContextVulkan* /*context*/) {
//    UpdateSurfaceConfig();
}

DawnSwapChainError XrSwapChainImplVk::Configure(WGPUTextureFormat format,
                                                  WGPUTextureUsage usage,
                                                  uint32_t width,
                                                  uint32_t height) {
//    UpdateSurfaceConfig();

//    ASSERT(mInfo.capabilities.minImageExtent.width <= width);
//    ASSERT(mInfo.capabilities.maxImageExtent.width >= width);
//    ASSERT(mInfo.capabilities.minImageExtent.height <= height);
//    ASSERT(mInfo.capabilities.maxImageExtent.height >= height);

//    ASSERT(format == static_cast<WGPUTextureFormat>(GetPreferredFormat()), "");
    // TODO(crbug.com/dawn/269): need to check usage works too

    //region Dawn swapchain
    // Create the swapchain with the configuration we chose
//    VkSwapchainKHR oldSwapchain = mSwapChain;
//    VkSwapchainCreateInfoKHR createInfo;
//    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
//    createInfo.pNext = nullptr;
//    createInfo.flags = 0;
//    createInfo.surface = mSurface;
//    createInfo.minImageCount = mConfig.minImageCount;
//    createInfo.imageFormat = mConfig.nativeFormat;
//    createInfo.imageColorSpace = mConfig.colorSpace;
//    createInfo.imageExtent.width = width;
//    createInfo.imageExtent.height = height;
//    createInfo.imageArrayLayers = 1;
//    createInfo.imageUsage = VulkanImageUsage(static_cast<wgpu::TextureUsage>(usage),
//                                             mDevice->GetValidInternalFormat(mConfig.format));
//    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
//    createInfo.queueFamilyIndexCount = 0;
//    createInfo.pQueueFamilyIndices = nullptr;
//    createInfo.preTransform = mConfig.preTransform;
//    createInfo.compositeAlpha = mConfig.compositeAlpha;
//    createInfo.presentMode = mConfig.presentMode;
//    createInfo.clipped = false;
//    createInfo.oldSwapchain = oldSwapchain;
//
//
//
//    if (mDevice->fn.CreateSwapchainKHR(mDevice->GetVkDevice(), &createInfo, nullptr,
//                                       &*mSwapChain) != VK_SUCCESS) {
//        ASSERT(false, "");
//    }
    //endregion

    auto configViews = g_OpenXRSessionManager->GetConfigViews();
    auto session = g_OpenXRSessionManager->GetSession();

    uint32_t swapchainFormatCount;
    CHECK_XRCMD(xrEnumerateSwapchainFormats(session, 0, &swapchainFormatCount, nullptr));
    std::vector<int64_t> swapchainFormats(swapchainFormatCount);
    CHECK_XRCMD(xrEnumerateSwapchainFormats(session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
                                            swapchainFormats.data()));
    CHECK(swapchainFormatCount == swapchainFormats.size());

    std::vector<int64_t> swapChainFormats = g_OpenXRSessionManager->GetSwapChainFormats();

    int64_t swapchainFormat = SelectColorSwapchainFormat(swapChainFormats);

    // Create a swapchain for each view
    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    createInfo.arraySize = 1;
    createInfo.format = swapchainFormat;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.mipCount = 1;
    createInfo.faceCount = 1;
    createInfo.sampleCount = configViews[g_CurrentXrView].recommendedSwapchainSampleCount;
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    CHECK_XRCMD(xrCreateSwapchain(session, &createInfo, &m_swapChain));

    uint32_t count = 0;
    CHECK_XRCMD(xrEnumerateSwapchainImages(m_swapChain, 0, &count, nullptr));
    m_swapChainImages.resize(count);
    m_swapChainBaseHeaders.resize(count);
    for (int i = 0; i < count; ++i) {
    m_swapChainImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR};
    m_swapChainBaseHeaders[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&m_swapChainImages[i]);
    }

    // Why do I need to use the first element of m_swapChainBaseHeaders like this?
    CHECK_XRCMD(xrEnumerateSwapchainImages(m_swapChain, count, &count, m_swapChainBaseHeaders[0]));

    return DAWN_SWAP_CHAIN_NO_ERROR;
}

DawnSwapChainError XrSwapChainImplVk::GetNextTexture(DawnSwapChainNextTexture* nextTexture) {
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t swapchainImageIndex;
    CHECK_XRCMD(xrAcquireSwapchainImage(m_swapChain, &acquireInfo, &swapchainImageIndex));

    // Don't know if I should put this here
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    CHECK_XRCMD(xrWaitSwapchainImage(m_swapChain, &waitInfo));

    // Looks like Dawn only works with one image at a time at the moment. So, just give it the first view image.
    nextTexture->texture.u64 = reinterpret_cast<uint64_t>(m_swapChainImages[swapchainImageIndex].image);

    return DAWN_SWAP_CHAIN_NO_ERROR;
}

/*
DawnSwapChainError XrSwapChainImplVk::GetNextTexture(DawnSwapChainNextTexture* nextTexture) {
    // Transiently create a semaphore that will be signaled when the presentation engine is done
    // with the swapchain image. Further operations on the image will wait for this semaphore.
    VkSemaphore semaphore = VK_NULL_HANDLE;
    {
        VkSemaphoreCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        if (mDevice->fn.CreateSemaphore(mDevice->GetVkDevice(), &createInfo, nullptr,
                                        &*semaphore) != VK_SUCCESS) {
            ASSERT(false, "");
        }
    }

    const auto result = mDevice->fn.AcquireNextImageKHR(
        mDevice->GetVkDevice(), mSwapChain, std::numeric_limits<uint64_t>::max(), semaphore,
        VkFence{}, &mLastImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return DAWN_SWAP_CHAIN_ERROR_OUT_OF_DATE;
    } else if (result != VK_SUCCESS) {
        ASSERT(false, "");
    }

    nextTexture->texture.u64 =
#if DAWN_PLATFORM_IS(64_BIT)
        reinterpret_cast<uint64_t>
#endif
        (*mSwapChainImages[mLastImageIndex]);
    mDevice->GetPendingRecordingContext()->waitSemaphores.push_back(semaphore);

    return DAWN_SWAP_CHAIN_NO_ERROR;
}
 */

/*
DawnSwapChainError XrSwapChainImplVk::Present() {
    // This assumes that the image has already been transitioned to the PRESENT layout and
    // writes were made available to the stage.

    // Assuming that the present queue is the same as the graphics queue, the proper
    // synchronization has already been done on the queue so we don't need to wait on any
    // semaphores.
    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 0;
    presentInfo.pWaitSemaphores = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &*mSwapChain;
    presentInfo.pImageIndices = &mLastImageIndex;
    presentInfo.pResults = nullptr;

    VkQueue queue = mDevice->GetQueue();
    const auto result = mDevice->fn.QueuePresentKHR(queue, &presentInfo);
    if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR) {
        ASSERT(false, "");
    }

    return DAWN_SWAP_CHAIN_NO_ERROR;
}
 */

DawnSwapChainError XrSwapChainImplVk::Present() {
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    CHECK_XRCMD(xrReleaseSwapchainImage(m_swapChain, &releaseInfo));
    return DAWN_SWAP_CHAIN_NO_ERROR;
}

wgpu::TextureFormat XrSwapChainImplVk::GetPreferredFormat() const {
    std::vector<int64_t> swapChainFormats = g_OpenXRSessionManager->GetSwapChainFormats();
    return ConvertToWgpuTextureFormat(SelectColorSwapchainFormat(swapChainFormats));
}

} // aurora::xr
