//
// Created by austin on 1/30/22.
// Derived from https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/tests/hello_xr/openxr_program.h
//

#pragma once

#include <memory>
#include <vector>

#include "aurora/xr/OpenXROptions.h"
#include <openxr/openxr.h>

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
  std::vector<XrViewConfigurationView> m_configViews;

public:
  bool xrEnabled = false;
  const XrInstance GetInstance() const;
  const XrSession GetSession() const;
  const XrSystemId GetSystemId() const;

  explicit OpenXRSessionManager(const OpenXROptions options);
  virtual ~OpenXRSessionManager() = default;

  std::vector<XrView> GetViews();

  uint32_t GetNumViews() {
      return m_views.size();
  };



  // Create an Instance and other basic instance-level initialization.
  bool createInstance(std::vector<std::string> instanceExtensions);

  //  // Select a System for the view configuration specified in the Options and initialize the graphics device for the
  //  // selected system.
  void initializeSystem();
  //
  // Create a Session and other basic session-level initialization.
  void initializeSession(XrBaseInStructure& graphicsBinding);

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

  static void LogLayersAndExtensions();

  static inline std::string GetXrVersionString(XrVersion ver);

  void LogInstanceInfo();

  void LogEnvironmentBlendMode(XrViewConfigurationType type);

  void LogViewConfigurations();
  std::vector<XrViewConfigurationView> GetConfigViews();
};

extern std::shared_ptr<OpenXRSessionManager> g_OpenXRSessionManager;

std::shared_ptr<OpenXRSessionManager> InstantiateOXRSessionManager(OpenXROptions options);

} // namespace boo