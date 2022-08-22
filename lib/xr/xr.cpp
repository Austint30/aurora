//
// Created by austin on 1/30/22.
// Derived from https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/tests/hello_xr/openxr_program.cpp
//
#include "aurora/xr/xr.hpp"
#include <cmath>
#include "check.h"
#include "log.hpp"

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

void OpenXRSessionManager::initializeSession(XrBaseInStructure& graphicsBinding) {
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

void OpenXRSessionManager::LogInstanceInfo() {
  if (m_instance == XR_NULL_HANDLE){
    Log.report(LOG_ERROR, FMT_STRING("XRInstance is null. Cannot log instance info."));
    return;
  }

  XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
  CHECK_XRCMD(xrGetInstanceProperties(m_instance, &instanceProperties));

  Log.report(LOG_INFO, FMT_STRING("Instance RuntimeName={} RuntimeVersion={}"), instanceProperties.runtimeName,
             GetXrVersionString(instanceProperties.runtimeVersion).c_str());
}

// static
void OpenXRSessionManager::LogLayersAndExtensions() {
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

void OpenXRSessionManager::LogEnvironmentBlendMode(XrViewConfigurationType type) {
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

void OpenXRSessionManager::LogViewConfigurations() {
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

// static
inline std::string OpenXRSessionManager::GetXrVersionString(XrVersion ver) {
  return Fmt("%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
}

std::shared_ptr<OpenXRSessionManager> InstantiateOXRSessionManager(OpenXROptions options){
  g_OpenXRSessionManager = std::make_shared<OpenXRSessionManager>(options);
  return g_OpenXRSessionManager;
}

} // aurora::xr