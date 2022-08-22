//
// Created by austin on 1/30/22.
// Derived from https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/tests/hello_xr/openxr_program.cpp
//
#include "xr.hpp"
#include <cmath>

namespace aurora::xr {

std::shared_ptr<OpenXRSessionManager> g_OpenXRSessionManager = nullptr;

inline XrFormFactor GetXrFormFactor(const std::string& formFactorStr) {
  if (EqualsIgnoreCase(formFactorStr, "Handheld")) {
    return XR_FORM_FACTOR_HANDHELD_DISPLAY;
  }
  if (!EqualsIgnoreCase(formFactorStr, "Hmd")) {
    Log.report(LOG_FATAL, FMT_STRING("Unknown form factor '{}'"), formFactorStr.c_str());
  }
  return XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
}

inline XrViewConfigurationType GetXrViewConfigurationType(const std::string& viewConfigurationStr) {
  if (EqualsIgnoreCase(viewConfigurationStr, "Mono")) {
    return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
  }
  if (!EqualsIgnoreCase(viewConfigurationStr, "Stereo")) {
    Log.report(LOG_FATAL, FMT_STRING("Unknown view configuration '{}'"), viewConfigurationStr.c_str());
  }

  return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
}

inline XrEnvironmentBlendMode GetXrEnvironmentBlendMode(const std::string& environmentBlendModeStr) {
  if (EqualsIgnoreCase(environmentBlendModeStr, "Additive")) {
    return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
  }
  if (EqualsIgnoreCase(environmentBlendModeStr, "AlphaBlend")) {
    return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
  }
  if (!EqualsIgnoreCase(environmentBlendModeStr, "Opaque")) {
    Log.report(LOG_FATAL, FMT_STRING("Unknown environment blend mode '{}'"), environmentBlendModeStr.c_str());
  }

  return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
}

namespace Math::Pose {
XrPosef Identity() {
  XrPosef t{};
  t.orientation.w = 1;
  return t;
}

XrPosef Translation(const XrVector3f& translation) {
  XrPosef t = Identity();
  t.position = translation;
  return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
  XrPosef t = Identity();
  t.orientation.x = 0.f;
  t.orientation.y = std::sin(radians * 0.5f);
  t.orientation.z = 0.f;
  t.orientation.w = std::cos(radians * 0.5f);
  t.position = translation;
  return t;
}
}  // namespace Math::Pose

inline XrReferenceSpaceCreateInfo GetXrReferenceSpaceCreateInfo(const std::string& referenceSpaceTypeStr) {
  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
  if (EqualsIgnoreCase(referenceSpaceTypeStr, "View")) {
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "ViewFront")) {
    // Render head-locked 2m in front of device.
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Translation({0.f, 0.f, -2.f}),
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Local")) {
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Stage")) {
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeft")) {
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRight")) {
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeftRotated")) {
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRightRotated")) {
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else {
    return referenceSpaceCreateInfo;
  }
  return referenceSpaceCreateInfo;
}

OpenXRSessionManager::OpenXRSessionManager(const OpenXROptions options)
: m_options(options) {
  m_views.resize(2, {XR_TYPE_VIEW});
}

bool OpenXRSessionManager::createInstance(std::vector<std::string> instanceExtensions) {
  LogLayersAndExtensions();

  CHECK(m_instance == XR_NULL_HANDLE);

  // Create union of extensions required by platform and graphics plugins.
  std::vector<const char*> extensions;

  // Transform platform and graphics extension std::strings to C strings.
  std::transform(instanceExtensions.begin(), instanceExtensions.end(), std::back_inserter(extensions),
                 [](const std::string& ext) { return ext.c_str(); });

  XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
  //  createInfo.next = m_application->GetInstanceCreateExtension();
  createInfo.enabledExtensionCount = (uint32_t)extensions.size();
  createInfo.enabledExtensionNames = extensions.data();

  strcpy(createInfo.applicationInfo.applicationName, m_options.AppName.c_str());
  createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  if (XR_FAILED(xrCreateInstance(&createInfo, &m_instance))){
    Log.report(LOG_FATAL, FMT_STRING("Failed to create OpenXR instance! Make sure your vr runtime is running!"));
    return false;
  }

  LogInstanceInfo();
  return true;
}

void OpenXRSessionManager::initializeSystem() {
  CHECK(m_instance != XR_NULL_HANDLE);
  CHECK(m_systemId == XR_NULL_SYSTEM_ID);

  m_formFactor = GetXrFormFactor(m_options.FormFactor);
  m_viewConfigType = GetXrViewConfigurationType(m_options.ViewConfiguration);
  m_environmentBlendMode = GetXrEnvironmentBlendMode(m_options.EnvironmentBlendMode);

  XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
  systemInfo.formFactor = m_formFactor;
  CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

  Log.report(LOG_INFO, FMT_STRING("Using system {} for form factor {}"), m_systemId, to_string(m_formFactor));
  CHECK(m_instance != XR_NULL_HANDLE);
  CHECK(m_systemId != XR_NULL_SYSTEM_ID);

  LogViewConfigurations();

  // The graphics API can initialize the graphics device now that the systemId and instance
  // handle are available.
  //  m_graphicsPlugin->InitializeDevice(m_instance, m_systemId);
}

void OpenXRSessionManager::initializeSession(XrGraphicsBindingVulkan2KHR& graphicsBinding) {
  CHECK(m_instance != XR_NULL_HANDLE);
  CHECK(m_session == XR_NULL_HANDLE);

  {
    Log.report(LOG_INFO, FMT_STRING("Creating session..."));

    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &graphicsBinding;
    createInfo.systemId = m_systemId;
    CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &m_session));
  }

  {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = GetXrReferenceSpaceCreateInfo(m_options.AppSpace);
    CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_appSpace));
  }
  //  InitializeActions();
}

std::vector<XrView> OpenXRSessionManager::GetViews() {
  XrResult res;

  XrViewState viewState{XR_TYPE_VIEW_STATE};
  uint32_t viewCapacityInput = (uint32_t)m_views.size();
  uint32_t viewCountOutput;

  XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
  viewLocateInfo.viewConfigurationType = m_viewConfigType;
  //    viewLocateInfo.displayTime = predictedDisplayTime;
  viewLocateInfo.space = m_appSpace;

  res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data());
  CHECK_XRRESULT(res, "xrLocateViews");
  if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    Throw("There is no valid tracking poses for the views.");
  }

  if (viewCountOutput != viewCapacityInput){
    Log.report(LOG_FATAL, FMT_STRING("viewCountOutput is not the same as viewCapacityInput!  viewCountOutput={}  viewCapacityInput={}"), viewCountOutput, viewCapacityInput);
  }

  return m_views;
}
const XrInstance OpenXRSessionManager::GetInstance() const { return m_instance; }
const XrSession OpenXRSessionManager::GetSession() const { return m_session; }
const XrSystemId OpenXRSessionManager::GetSystemId() const { return m_systemId; }

std::shared_ptr<OpenXRSessionManager> InstantiateOXRSessionManager(OpenXROptions options){
  g_OpenXRSessionManager = std::make_shared<OpenXRSessionManager>(options);
  return g_OpenXRSessionManager;
}

} // aurora::xr