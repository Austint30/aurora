//
// Created by austin on 1/30/22.
// Derived from https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/tests/hello_xr/openxr_program.h
//

#pragma once

#include <memory>
#include <vector>

#include "OpenXROptions.h"
#include "check.h"
#include "../dawn/BackendBinding.hpp"
#include "log.hpp"

namespace aurora::xr {
struct OpenXRSessionManager {
  friend class WindowXlib;
private:
  const OpenXROptions m_options;
  XrInstance m_instance{XR_NULL_HANDLE};
  XrSession m_session{XR_NULL_HANDLE};
  XrSpace m_appSpace{XR_NULL_HANDLE};
  XrFormFactor m_formFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
  XrViewConfigurationType m_viewConfigType{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
  XrEnvironmentBlendMode m_environmentBlendMode{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
  std::vector<XrView> m_views;

public:
  explicit OpenXRSessionManager(const OpenXROptions options);
  virtual ~OpenXRSessionManager() = default;

  std::vector<XrView> GetViews();

  // Create an Instance and other basic instance-level initialization.
  bool createInstance(webgpu::utils::BackendBinding& backendBinding);

  //  // Select a System for the view configuration specified in the Options and initialize the graphics device for the
  //  // selected system.
  void initializeSystem();
  //
  // Create a Session and other basic session-level initialization.
  void initializeSession(webgpu::utils::BackendBinding& backendBinding);

  //  // Create a Swapchain which requires coordinating with the graphics plugin to select the format, getting the system
  //  // graphics properties, getting the view configuration and grabbing the resulting swapchain images.
  //  virtual void CreateSwapchains() = 0;
  //
  //  // Process any events in the event queue.
  //  virtual void PollEvents(bool* exitRenderLoop, bool* requestRestart) = 0;
  //
  //  // Manage session lifecycle to track if RenderFrame should be called.
  //  virtual bool IsSessionRunning() const = 0;
  //
  //  // Manage session state to track if input should be processed.
  //  virtual bool IsSessionFocused() const = 0;
  //
  //  // Sample input actions and generate haptic feedback.
  //  virtual void PollActions() = 0;
  //
  //  // Create and submit a frame.
  //  virtual void RenderFrame() = 0;

  static void LogLayersAndExtensions() {
    // Write out extension properties for a given layer.
    const auto logExtensions = [](const char* layerName, int indent = 0) {
      uint32_t instanceExtensionCount;
      CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));

      std::vector<XrExtensionProperties> extensions(instanceExtensionCount);
      for (XrExtensionProperties& extension : extensions) {
        extension.type = XR_TYPE_EXTENSION_PROPERTIES;
      }

      CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(),
                                                         &instanceExtensionCount, extensions.data()));

      const std::string indentStr(indent, ' ');

      Log.report(LOG_INFO, FMT_STRING("{}Available Extensions: {}"), indentStr.c_str(), instanceExtensionCount);
      for (const XrExtensionProperties& extension : extensions) {
        Log.report(LOG_INFO, FMT_STRING("{}  Name={} SpecVersion={}"), indentStr.c_str(), extension.extensionName,
                   extension.extensionVersion);
      }
    };

    // Log non-layer extensions (layerName==nullptr).
    logExtensions(nullptr);

    // Log layers and any of their extensions.
    {
      uint32_t layerCount;
      CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));

      std::vector<XrApiLayerProperties> layers(layerCount);
      for (XrApiLayerProperties& layer : layers) {
        layer.type = XR_TYPE_API_LAYER_PROPERTIES;
      }

      CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

      Log.report(LOG_INFO, FMT_STRING("Available Layers: ({})"), layerCount);
      for (const XrApiLayerProperties& layer : layers) {
        Log.report(LOG_INFO, FMT_STRING("  Name={} SpecVersion={} LayerVersion={} Description={}"), layer.layerName,
                   GetXrVersionString(layer.specVersion).c_str(), layer.layerVersion, layer.description);
        logExtensions(layer.layerName, 4);
      }
    }
  }

  static inline std::string GetXrVersionString(XrVersion ver) {
    return Fmt("%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
  }

  void LogInstanceInfo() {
    if (m_instance == XR_NULL_HANDLE){
      Log.report(LOG_ERROR, FMT_STRING("XRInstance is null. Cannot log instance info."));
      return;
    }

    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    CHECK_XRCMD(xrGetInstanceProperties(m_instance, &instanceProperties));

    Log.report(LOG_INFO, FMT_STRING("Instance RuntimeName={} RuntimeVersion={}"), instanceProperties.runtimeName,
               GetXrVersionString(instanceProperties.runtimeVersion).c_str());
  }

  void LogEnvironmentBlendMode(XrViewConfigurationType type) {
    CHECK(m_instance != XR_NULL_HANDLE);
    CHECK(m_systemId != 0);

    uint32_t count;
    CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, 0, &count, nullptr));
    CHECK(count > 0);

    Log.report(LOG_INFO, FMT_STRING("Available Environment Blend Mode count : ({})"), count);

    std::vector<XrEnvironmentBlendMode> blendModes(count);
    CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, count, &count, blendModes.data()));

    bool blendModeFound = false;
    for (XrEnvironmentBlendMode mode : blendModes) {
      const bool blendModeMatch = (mode == m_environmentBlendMode);
      Log.report(LOG_INFO,
                 FMT_STRING("Environment Blend Mode ({}) : {}"), to_string(mode), blendModeMatch ? "(Selected)" : "");
      blendModeFound |= blendModeMatch;
    }
    CHECK(blendModeFound);
  }

  void LogViewConfigurations() {
    CHECK(m_instance != XR_NULL_HANDLE);
    CHECK(m_systemId != XR_NULL_SYSTEM_ID);

    uint32_t viewConfigTypeCount;
    CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigTypeCount, nullptr));
    std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);
    CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigTypeCount, &viewConfigTypeCount,
                                              viewConfigTypes.data()));
    CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);

    Log.report(LOG_INFO, FMT_STRING("Available View Configuration Types: ({})"), viewConfigTypeCount);
    for (XrViewConfigurationType viewConfigType : viewConfigTypes) {
      Log.report(LOG_INFO, FMT_STRING("  View Configuration Type: {} {}"), to_string(viewConfigType),
                 viewConfigType == m_viewConfigType ? "(Selected)" : "");

      XrViewConfigurationProperties viewConfigProperties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
      CHECK_XRCMD(xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigType, &viewConfigProperties));

      Log.report(LOG_INFO,
                 FMT_STRING("  View configuration FovMutable={}"), viewConfigProperties.fovMutable == XR_TRUE ? "True" : "False");

      uint32_t viewCount;
      CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, 0, &viewCount, nullptr));
      if (viewCount > 0) {
        std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        CHECK_XRCMD(
            xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, viewCount, &viewCount, views.data()));

        for (uint32_t i = 0; i < views.size(); i++) {
          const XrViewConfigurationView& view = views[i];

          Log.report(LOG_INFO, FMT_STRING("    View [{}]: Recommended Width={} Height={} SampleCount={}"), i,
                     view.recommendedImageRectWidth, view.recommendedImageRectHeight,
                     view.recommendedSwapchainSampleCount);
          Log.report(LOG_INFO,
                     FMT_STRING("    View [{}]:     Maximum Width={} Height={} SampleCount={}"), i, view.maxImageRectWidth,
                     view.maxImageRectHeight, view.maxSwapchainSampleCount);
        }
      } else {
        Log.report(LOG_ERROR, FMT_STRING("Empty view configuration type"));
      }

      LogEnvironmentBlendMode(viewConfigType);
    }
  }
};

extern std::shared_ptr<OpenXRSessionManager> g_OpenXRSessionManager;

std::shared_ptr<OpenXRSessionManager> InstantiateOXRSessionManager(OpenXROptions options);

} // namespace boo