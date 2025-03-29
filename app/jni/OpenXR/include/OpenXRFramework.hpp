#ifndef OPENXR_FRAMEWORK_HPP
#define OPENXR_FRAMEWORK_HPP

#include "Platform/platform.hpp"

#include "lib/GraphicsPlugin.hpp"

#include <array>
#include <map>

struct Swapchain {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
};

struct ARInputState {
    XrActionSet action_set = XR_NULL_HANDLE;
    XrAction touch_action = XR_NULL_HANDLE;
    XrAction quit_action = XR_NULL_HANDLE;
};

class OpenXRFramework {
public:
    explicit OpenXRFramework(std::shared_ptr<Platform> platform);
    ~OpenXRFramework();

    // Initialization methods
    XrResult CreateInstance();
    XrResult CreateDebugMessenger();
    XrResult GetInstanceProperties() const;
    XrResult GetSystemID();

    XrResult CreateActionSet();
    XrResult SuggestBindings() const;

    XrResult GetViewConfigurationViews();
    XrResult GetEnvironmentBlendModes();

    XrResult CreateSession();

    XrResult CreateActionPoses();
    XrResult AttachActionSet();

    XrResult CreateReferenceSpace();
    XrResult CreateSwapchains();

    XrResult CreateResources();

    // Runtime methods
    XrResult PollSystemEvents();
    XrResult PollEvents();
    XrResult RenderFrame();

    bool IsSessionRunning() const { return session_running_; }

    // Cleanup methods
    XrResult DestroyOpenXRFramework();
    XrResult DestroySwapChains();
    XrResult DestroyReferenceSpace();
    XrResult DestroyResources();
    XrResult DestroySession();
    XrResult DestroyDebugMessenger();
    XrResult DestroyInstance();

private:
    // Helper methods
    std::vector<int64_t> GetSupportedSwapchainFormats();
    const XrEventDataBaseHeader* TryReadNextEvent();
    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& state_changed_event);
    bool RenderLayer(XrTime predicted_display_time,
                     std::vector<XrCompositionLayerProjectionView>& projection_layer_views,
                     XrCompositionLayerProjection& layer);

    // Platform and graphics
    std::shared_ptr<Platform> platform_;
    std::shared_ptr<GraphicsPlugin> graphics_plugin_;

    // OpenXR handles
    XrInstance instance_ = XR_NULL_HANDLE;
    XrDebugUtilsMessengerEXT debug_messenger_ = XR_NULL_HANDLE;
    XrSystemId system_id_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace app_space_ = XR_NULL_HANDLE;

    // AR configuration
    XrViewConfigurationType view_config_type_ = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO; // For AR
    XrEnvironmentBlendMode blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND; // For AR
    std::vector<XrViewConfigurationView> config_views_;
    std::vector<XrView> views_;

    // Swapchain management
    std::vector<Swapchain> swapchains_;
    std::map<XrSwapchain, XrSwapchainImageBaseHeader*> swapchain_images_;

    // Tracking spaces
    std::vector<XrSpace> visualized_spaces_;

    // Input handling
    ARInputState input_{};

    // Session state tracking
    XrEventDataBuffer event_data_buffer_{};
    XrSessionState session_state_ = XR_SESSION_STATE_UNKNOWN;
    bool session_running_ = false;
};

std::shared_ptr<OpenXRFramework> CreateOpenXRFramework(std::shared_ptr<Platform> platform);

#endif //OPENXR_FRAMEWORK_HPP
