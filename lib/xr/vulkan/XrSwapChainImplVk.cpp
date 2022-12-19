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
#include "../xr.hpp"

namespace aurora::xr {

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
    UpdateSurfaceConfig();
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

void XrSwapChainImplVk::UpdateSurfaceConfig() {
//    if (mDevice->ConsumedError(GatherSurfaceInfo(*ToBackend(mDevice->GetAdapter()), mSurface),
//                               &mInfo)) {
//        ASSERT(false, "");
//    }
//
//    if (!ChooseSurfaceConfig(mInfo, &mConfig, mDevice->IsToggleEnabled(Toggle::TurnOffVsync))) {
//        ASSERT(false, "");
//    }
}

void XrSwapChainImplVk::Init(DawnWSIContextVulkan* /*context*/) {
    UpdateSurfaceConfig();
}

DawnSwapChainError XrSwapChainImplVk::Configure(WGPUTextureFormat format,
                                                  WGPUTextureUsage usage,
                                                  uint32_t width,
                                                  uint32_t height) {
    UpdateSurfaceConfig();

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

    // Create a swapchain for each view
    for (auto & configView : configViews) {
        XrSwapchain swapChain = nullptr;
        XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.arraySize = 1;
        createInfo.format = VK_FORMAT_B8G8R8A8_UNORM; // Hard coding for now
        createInfo.width = configView.recommendedImageRectWidth;
        createInfo.height = configView.recommendedImageRectHeight;
        createInfo.mipCount = 1;
        createInfo.faceCount = 1;
        createInfo.sampleCount = configView.recommendedSwapchainSampleCount;
        createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        CHECK_XRCMD(xrCreateSwapchain(session, &createInfo, &swapChain));

        m_swapChains.push_back(swapChain);
    }

    for (const auto & swapChain : m_swapChains){
        uint32_t count = 0;
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapChain, 0, &count, nullptr));
        m_swapChainImages.resize(count);
        m_swapChainBaseHeaders.resize(count);
        for (int i = 0; i < count; ++i) {
            m_swapChainImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR};
            m_swapChainBaseHeaders[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&m_swapChainImages[i]);
        }

        // Why do I need to use the first element of m_swapChainBaseHeaders like this?
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapChain, count, &count, m_swapChainBaseHeaders[0]));
    }

    return DAWN_SWAP_CHAIN_NO_ERROR;
}

DawnSwapChainError XrSwapChainImplVk::GetNextTexture(DawnSwapChainNextTexture* nextTexture) {

    std::vector<uint32_t> imageIndexes(m_swapChains.size());

    for (int i = 0; i < m_swapChains.size(); ++i) {
        XrSwapchain swapChain = m_swapChains[i];
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t swapchainImageIndex;
        CHECK_XRCMD(xrAcquireSwapchainImage(swapChain, &acquireInfo, &swapchainImageIndex));

        imageIndexes[i] = swapchainImageIndex;

        // Don't know if I should put this here
        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        CHECK_XRCMD(xrWaitSwapchainImage(swapChain, &waitInfo));
    }

    // Looks like Dawn only works with one image at a time at the moment. So, just give it the first view image.
    nextTexture->texture.u64 = reinterpret_cast<uint64_t>(&m_swapChainImages[imageIndexes[0]]);

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
    for (int i = 0; i < m_swapChains.size(); ++i) {
        XrSwapchain swapChain = m_swapChains[i];

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        CHECK_XRCMD(xrReleaseSwapchainImage(swapChain, &releaseInfo));
    }
}

wgpu::TextureFormat XrSwapChainImplVk::GetPreferredFormat() const {
    return wgpu::TextureFormat::BGRA8Unorm;
}

} // aurora::xr
