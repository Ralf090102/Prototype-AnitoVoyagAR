#include "OpenXRFramework.hpp"

#include "Platform/platform.hpp"
#include "lib/GraphicsPlugin.hpp"
#include "OpenXRUtils.hpp"
#include "magic_enum/magic_enum.hpp"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

namespace {
    bool EqualsIgnoreCase(const std::string &a, const std::string &b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) {
            return tolower(a) == tolower(b);
        });
    }
};

OpenXRFramework::OpenXRFramework(std::shared_ptr<Platform> platform): platform_(std::move(platform)) {
    graphics_plugin_ = CreateGraphicsPlugin();
}

OpenXRFramework::~OpenXRFramework() {
    DestroyOpenXRFramework();
}

XrResult OpenXRFramework::CreateInstance() {
    // Log available extensions and layers
    LogLayersAndExtensions();

    if (instance_ != XR_NULL_HANDLE) {
        throw std::runtime_error("xr instance must not have been inited");
    }

    std::vector<const char*> enabled_extensions;

    // Get platform-specific extensions
    const std::vector<std::string>& platform_extensions = platform_->GetInstanceExtensions();
    std::transform(platform_extensions.begin(),platform_extensions.end(),
                   std::back_inserter(enabled_extensions),
                   [](const std::string& ext) { return ext.c_str(); });

    // Get graphics API extensions
    const std::vector<std::string>& graphics_extensions = graphics_plugin_->GetOpenXrInstanceExtensions();
    std::transform(graphics_extensions.begin(), graphics_extensions.end(),
                   std::back_inserter(enabled_extensions),
                   [](const std::string& ext) { return ext.c_str(); });

    // Add debug extension if needed
    enabled_extensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Add ARCore-specific extensions
    enabled_extensions.push_back("XR_GOOGLE_display_timing");
    enabled_extensions.push_back("XR_KHR_composition_layer_cylinder");
    enabled_extensions.push_back("XR_KHR_composition_layer_equirect2");
    enabled_extensions.push_back("XR_MSFT_unbounded_reference_space");

    // Create the OpenXR instance
    XrApplicationInfo app_info{};
    app_info.apiVersion = XR_CURRENT_API_VERSION;
    strcpy(app_info.applicationName, "AR Application");
    app_info.applicationVersion = 1;
    strcpy(app_info.engineName, "Anito VoyagAR Engine");
    app_info.engineVersion = 1;

    XrInstanceCreateInfo create_info{};
    create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
    create_info.enabledExtensionNames = enabled_extensions.data();
    create_info.next = platform_->GetInstanceCreateExtension();
    create_info.applicationInfo = app_info;

    CHECK_XRCMD(xrCreateInstance(&create_info, &instance_));

    LogInstanceInfo(instance_);

    return GetInstanceProperties();
}

XrResult OpenXRFramework::CreateDebugMessenger() {
    XrDebugUtilsMessengerCreateInfoEXT debug_info{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_info.messageSeverities =
            XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageTypes =
            XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.userCallback = [](XrDebugUtilsMessageSeverityFlagsEXT severity,
                                 XrDebugUtilsMessageTypeFlagsEXT types,
                                 const XrDebugUtilsMessengerCallbackDataEXT* data,
                                 void* user_data) -> XrBool32 {
        spdlog::info("XR_DEBUG: {}", data->message);
        return XR_FALSE;
    };

    PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT = nullptr;
    CHECK_XRCMD(xrGetInstanceProcAddr(instance_, "xrCreateDebugUtilsMessengerEXT",
                                      reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateDebugUtilsMessengerEXT)));
    CHECK_XRCMD(xrCreateDebugUtilsMessengerEXT(instance_, &debug_info, &debug_messenger_));

    return XR_SUCCESS;
}

XrResult OpenXRFramework::GetInstanceProperties() const {
    LogInstanceInfo(instance_);
    return XR_SUCCESS;
}

XrResult OpenXRFramework::GetSystemID() {
    if (instance_ == XR_NULL_HANDLE) {
        throw std::runtime_error("instance is xr null handle");
    }
    if (system_id_ != XR_NULL_SYSTEM_ID) {
        throw std::runtime_error("system id must be null system id");
    }

    XrSystemGetInfo system_info{};
    system_info.type = XR_TYPE_SYSTEM_GET_INFO;
    system_info.formFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY; // For mobile AR
    CHECK_XRCMD(xrGetSystem(instance_, &system_info, &system_id_));

    if (system_id_ == XR_NULL_SYSTEM_ID) {
        throw std::runtime_error("system id must not be null system id");
    }

    // Log system properties
    LogSystemProperties(instance_, system_id_);

    return XR_SUCCESS;
}

XrResult OpenXRFramework::CreateActionSet() {
    // For AR on phones, we might not need complex action sets like for VR controllers
    // But we'll set up a simple action set for touch interactions
    XrActionSetCreateInfo action_set_info{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy(action_set_info.actionSetName, "ar_input");
    strcpy(action_set_info.localizedActionSetName, "AR Input");
    action_set_info.priority = 0;
    CHECK_XRCMD(xrCreateActionSet(instance_, &action_set_info, &input_.action_set));

    // Create actions for touch/tap and quit
    XrActionCreateInfo action_info{XR_TYPE_ACTION_CREATE_INFO};
    action_info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy(action_info.actionName, "touch");
    strcpy(action_info.localizedActionName, "Touch");
    CHECK_XRCMD(xrCreateAction(input_.action_set, &action_info, &input_.touch_action));

    strcpy(action_info.actionName, "quit");
    strcpy(action_info.localizedActionName, "Quit");
    CHECK_XRCMD(xrCreateAction(input_.action_set, &action_info, &input_.quit_action));

    return XR_SUCCESS;
}

XrResult OpenXRFramework::SuggestBindings() const {
    // For phone AR, we might bind to touch screen inputs
    // This is a simplified version - you might need to adapt based on your needs
    XrPath touch_interaction_profile;
    CHECK_XRCMD(xrStringToPath(instance_, "/interaction_profiles/khr/simple_controller", &touch_interaction_profile));

    std::vector<XrActionSuggestedBinding> bindings;
    XrActionSuggestedBinding binding{};

    // Bind the touch action to the select click
    binding.action = input_.touch_action;
    CHECK_XRCMD(xrStringToPath(instance_, "/user/hand/right/input/select/click", &binding.binding));
    bindings.push_back(binding);

    // Bind the quit action
    binding.action = input_.quit_action;
    CHECK_XRCMD(xrStringToPath(instance_, "/user/hand/right/input/menu/click", &binding.binding));
    bindings.push_back(binding);

    XrInteractionProfileSuggestedBinding suggested_bindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested_bindings.interactionProfile = touch_interaction_profile;
    suggested_bindings.suggestedBindings = bindings.data();
    suggested_bindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
    CHECK_XRCMD(xrSuggestInteractionProfileBindings(instance_, &suggested_bindings));

    return XR_SUCCESS;
}

XrResult OpenXRFramework::GetViewConfigurationViews() {
    // For AR, we typically use mono view configuration
    uint32_t view_count = 0;
    CHECK_XRCMD(xrEnumerateViewConfigurationViews(instance_, system_id_, view_config_type_, 0, &view_count, nullptr));

    config_views_.resize(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    CHECK_XRCMD(xrEnumerateViewConfigurationViews(
            instance_, system_id_, view_config_type_, view_count, &view_count, config_views_.data()));

    views_.resize(view_count, {XR_TYPE_VIEW});

    return XR_SUCCESS;
}

XrResult OpenXRFramework::GetEnvironmentBlendModes() {
    // Find supported blend modes
    uint32_t count = 0;
    CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(
            instance_, system_id_, view_config_type_, 0, &count, nullptr));

    std::vector<XrEnvironmentBlendMode> blend_modes(count);
    CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(
            instance_, system_id_, view_config_type_, count, &count, blend_modes.data()));

    // Choose the appropriate blend mode for AR
    blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
    bool found_blend_mode = false;

    for (XrEnvironmentBlendMode mode : blend_modes) {
        if (mode == blend_mode_) {
            found_blend_mode = true;
            break;
        }
    }

    if (!found_blend_mode) {
        spdlog::error("Required blend mode not supported");
        return XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED;
    }

    return XR_SUCCESS;
}

XrResult OpenXRFramework::CreateSession() {
    if (instance_ == XR_NULL_HANDLE) {
        throw std::runtime_error("instance_ can not be xr null handle");
    }
    if (session_ != XR_NULL_HANDLE) {
        throw std::runtime_error("session must be xr null handle");
    }
    // Initialize graphics device with OpenXR instance
    graphics_plugin_->InitializeDevice(instance_, system_id_);

    // Create session with graphics binding
    XrSessionCreateInfo session_create_info{};
    session_create_info.type = XR_TYPE_SESSION_CREATE_INFO;
    session_create_info.next = graphics_plugin_->GetGraphicsBinding();
    session_create_info.systemId = system_id_;
    CHECK_XRCMD(xrCreateSession(instance_, &session_create_info, &session_));

    // Log available reference spaces
    LogReferenceSpaces(session_);

    return XR_SUCCESS;
}

XrResult OpenXRFramework::CreateActionPoses() {
    // For phone AR, we don't typically create action poses for controllers
    // This is a placeholder for AR-specific pose tracking if needed
    return XR_SUCCESS;
}

XrResult OpenXRFramework::AttachActionSet() {
    XrSessionActionSetsAttachInfo attach_info{};
    attach_info.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    attach_info.countActionSets = 1;
    attach_info.actionSets = &input_.action_set;
    CHECK_XRCMD(xrAttachSessionActionSets(session_, &attach_info));

    return XR_SUCCESS;
}

XrResult OpenXRFramework::CreateReferenceSpace() {
    // For AR, we typically use STAGE reference space
    XrReferenceSpaceCreateInfo reference_space_create_info{};
    reference_space_create_info.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    reference_space_create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    reference_space_create_info.poseInReferenceSpace.orientation.w = 1.0f;
    CHECK_XRCMD(xrCreateReferenceSpace(session_, &reference_space_create_info, &app_space_));

    return XR_SUCCESS;
}

XrResult OpenXRFramework::CreateSwapchains() {
    // For AR, we typically need only one swapchain (mono view)
    swapchains_.resize(config_views_.size());
    swapchain_images_.clear();

    for (size_t i = 0; i < config_views_.size(); i++) {
        XrSwapchainCreateInfo swapchain_create_info{};
        swapchain_create_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_create_info.format = graphics_plugin_->SelectSwapchainFormat(GetSupportedSwapchainFormats());
        swapchain_create_info.sampleCount = config_views_[i].recommendedSwapchainSampleCount;
        swapchain_create_info.width = config_views_[i].recommendedImageRectWidth;
        swapchain_create_info.height = config_views_[i].recommendedImageRectHeight;
        swapchain_create_info.faceCount = 1;
        swapchain_create_info.arraySize = 1;
        swapchain_create_info.mipCount = 1;

        swapchains_[i].width = swapchain_create_info.width;
        swapchains_[i].height = swapchain_create_info.height;

        CHECK_XRCMD(xrCreateSwapchain(session_, &swapchain_create_info, &swapchains_[i].handle));

        // Get images from swapchain
        uint32_t image_count;
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchains_[i].handle, 0, &image_count, nullptr));

        XrSwapchainImageBaseHeader* images = graphics_plugin_->AllocateSwapchainImageStructs(
                image_count, swapchain_create_info);
        CHECK_XRCMD(xrEnumerateSwapchainImages(
                swapchains_[i].handle, image_count, &image_count, images));

        swapchain_images_[swapchains_[i].handle] = images;
        graphics_plugin_->SwapchainImageStructsReady(images);
    }

    return XR_SUCCESS;
}

XrResult OpenXRFramework::CreateResources() {
    // Create any additional resources needed
    return XR_SUCCESS;
}

XrResult OpenXRFramework::PollSystemEvents() {
    // Handle Android system events if needed
    return XR_SUCCESS;
}

const XrEventDataBaseHeader* OpenXRFramework::TryReadNextEvent() {
    XrEventDataBaseHeader* base_header = reinterpret_cast<XrEventDataBaseHeader*>(&event_data_buffer_);
    base_header->type = XR_TYPE_EVENT_DATA_BUFFER;
    XrResult result = xrPollEvent(instance_, &event_data_buffer_);

    if (result == XR_SUCCESS) {
        if (base_header->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
            auto events_lost = reinterpret_cast<XrEventDataEventsLost *>(base_header);
            spdlog::warn("{} events lost", events_lost->lostEventCount);
        }
        return base_header;
    }

    if (result != XR_EVENT_UNAVAILABLE) {
        spdlog::error("xr pull event unknown result: {}", magic_enum::enum_name(result));
        return nullptr;
    }

    CHECK_XRCMD(result);
    return nullptr;
}

void OpenXRFramework::HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& state_changed_event) {

    spdlog::info("XrEventDataSessionStateChanged: state {}->{} time={}",
                 magic_enum::enum_name(session_state_),
                 magic_enum::enum_name(state_changed_event.state),
                 state_changed_event.time);

    if ((state_changed_event.session != XR_NULL_HANDLE) && (state_changed_event.session != session_)) {
        spdlog::error("XrEventDataSessionStateChanged for unknown session");
        return;
    }

    session_state_ = state_changed_event.state;
    switch (session_state_) {
        case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo session_begin_info{XR_TYPE_SESSION_BEGIN_INFO};
            session_begin_info.primaryViewConfigurationType = view_config_type_;
            CHECK_XRCMD(xrBeginSession(session_, &session_begin_info));
            session_running_ = true;
            break;
        }
        case XR_SESSION_STATE_STOPPING: {
            CHECK_XRCMD(xrEndSession(session_));
            session_running_ = false;
            break;
        }
        default:
            break;
    }
}

XrResult OpenXRFramework::PollEvents() {
    while (auto event = TryReadNextEvent()) {
        switch (event->type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto state_event = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
                HandleSessionStateChangedEvent(state_event);
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                const auto &instance_loss_pending =
                        *reinterpret_cast<const XrEventDataInstanceLossPending *>(&event);
                spdlog::warn("XrEventDataInstanceLossPending by {}", instance_loss_pending.lossTime);
                break;
            }
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                    //TODO
                    break;
            }
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {}
            default: {
                spdlog::debug("Ignoring event type {}", magic_enum::enum_name(event->type));
                break;
            }
        }
    }
    return XR_SUCCESS;
}

bool OpenXRFramework::RenderLayer(
        XrTime predicted_display_time,
        std::vector<XrCompositionLayerProjectionView>& projection_layer_views,
        XrCompositionLayerProjection& layer) {

    // Locate views
    XrViewLocateInfo view_locate_info{XR_TYPE_VIEW_LOCATE_INFO};
    view_locate_info.viewConfigurationType = view_config_type_;
    view_locate_info.displayTime = predicted_display_time;
    view_locate_info.space = app_space_;

    XrViewState view_state{};
    view_state.type = XR_TYPE_VIEW_STATE;
    uint32_t view_count_output = 0;
    CHECK_XRCMD(xrLocateViews(
            session_, &view_locate_info, &view_state, views_.size(), &view_count_output, views_.data()));

    if ((view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
        (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
        return false;  // There is no valid tracking
    }

    // Render each view to its corresponding swapchain image
    for (uint32_t i = 0; i < view_count_output; i++) {
        // Each view has a separate swapchain
        const Swapchain& swapchain = swapchains_[i];

        // Acquire swapchain image
        XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t image_index;
        CHECK_XRCMD(xrAcquireSwapchainImage(swapchain.handle, &acquire_info, &image_index));

        // Wait for the image to be available
        XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait_info.timeout = XR_INFINITE_DURATION;
        CHECK_XRCMD(xrWaitSwapchainImage(swapchain.handle, &wait_info));

        // Set up projection view
        projection_layer_views[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projection_layer_views[i].pose = views_[i].pose;
        projection_layer_views[i].fov = views_[i].fov;
        projection_layer_views[i].subImage.swapchain = swapchain.handle;
        projection_layer_views[i].subImage.imageRect.offset = {0, 0};
        projection_layer_views[i].subImage.imageRect.extent = {swapchain.width, swapchain.height};

        // Get swapchain images
        auto* const swapchain_image_base_header = swapchain_images_[swapchain.handle];

        // Render content to the view
        // This is where you'll integrate your Vulkan rendering code
        std::vector<math::Transform> cube_transforms; // Placeholder for your scene objects
        graphics_plugin_->RenderView(
                projection_layer_views[i],
                swapchain_image_base_header,
                image_index,
                cube_transforms);

        // Release the swapchain image
        XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        CHECK_XRCMD(xrReleaseSwapchainImage(swapchain.handle, &release_info));
    }

    // Set up the layer itself
    layer.space = app_space_;
    layer.viewCount = (uint32_t)projection_layer_views.size();
    layer.views = projection_layer_views.data();

    return true;
}

XrResult OpenXRFramework::RenderFrame() {
    if (!session_running_) {
        return XR_SUCCESS;
    }

    // Wait for a predicted display time
    XrFrameWaitInfo frame_wait_info{XR_TYPE_FRAME_WAIT_INFO, 0};
    XrFrameState frame_state{};
    frame_state.type = XR_TYPE_FRAME_STATE;
    CHECK_XRCMD(xrWaitFrame(session_, &frame_wait_info, &frame_state));

    // Begin the frame
    XrFrameBeginInfo frame_begin_info{XR_TYPE_FRAME_BEGIN_INFO, 0};
    CHECK_XRCMD(xrBeginFrame(session_, &frame_begin_info));

    // Render layers
    std::vector<XrCompositionLayerBaseHeader*> layers;

    // Set up projection layer views
    std::vector<XrCompositionLayerProjectionView> projection_layer_views(views_.size());
    XrCompositionLayerProjection projection_layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};

    if (frame_state.shouldRender && RenderLayer(frame_state.predictedDisplayTime, projection_layer_views, projection_layer)) {
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&projection_layer));
    }

    // End frame
    XrFrameEndInfo frame_end_info{XR_TYPE_FRAME_END_INFO};
    frame_end_info.displayTime = frame_state.predictedDisplayTime;
    frame_end_info.environmentBlendMode = blend_mode_;
    frame_end_info.layerCount = (uint32_t)layers.size();
    frame_end_info.layers = layers.data();
    CHECK_XRCMD(xrEndFrame(session_, &frame_end_info));

    return XR_SUCCESS;
}

// Helper function to get supported swapchain formats
std::vector<int64_t> OpenXRFramework::GetSupportedSwapchainFormats() {
    uint32_t format_count;
    CHECK_XRCMD(xrEnumerateSwapchainFormats(session_, 0, &format_count, nullptr));

    std::vector<int64_t> formats(format_count);
    CHECK_XRCMD(xrEnumerateSwapchainFormats(session_, format_count, &format_count, formats.data()));
    return formats;
}

// Destruction methods
XrResult OpenXRFramework::DestroyOpenXRFramework() {
    DestroySwapChains();
    DestroyReferenceSpace();
    DestroyResources();
    DestroySession();
    DestroyDebugMessenger();
    DestroyInstance();
    return XR_SUCCESS;
}

XrResult OpenXRFramework::DestroySwapChains() {
    for (auto& swapchain : swapchains_) {
        if (swapchain.handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(swapchain.handle);
            swapchain.handle = XR_NULL_HANDLE;
        }
    }
    swapchains_.clear();
    swapchain_images_.clear();
    return XR_SUCCESS;
}

XrResult OpenXRFramework::DestroyReferenceSpace() {
    if (app_space_ != XR_NULL_HANDLE) {
        xrDestroySpace(app_space_);
        app_space_ = XR_NULL_HANDLE;
    }
    return XR_SUCCESS;
}

XrResult OpenXRFramework::DestroyResources() {
    // Destroy any additional resources
    return XR_SUCCESS;
}

XrResult OpenXRFramework::DestroySession() {
    if (session_ != XR_NULL_HANDLE) {
        if (session_running_) {
            CHECK_XRCMD(xrEndSession(session_));
            session_running_ = false;
        }
        xrDestroySession(session_);
        session_ = XR_NULL_HANDLE;
    }
    return XR_SUCCESS;
}

XrResult OpenXRFramework::DestroyDebugMessenger() {
    if (debug_messenger_ != XR_NULL_HANDLE) {
        PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance_, "xrDestroyDebugUtilsMessengerEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyDebugUtilsMessengerEXT)));
        CHECK_XRCMD(xrDestroyDebugUtilsMessengerEXT(debug_messenger_));
        debug_messenger_ = XR_NULL_HANDLE;
    }
    return XR_SUCCESS;
}

XrResult OpenXRFramework::DestroyInstance() {
    if (instance_ != XR_NULL_HANDLE) {
        xrDestroyInstance(instance_);
        instance_ = XR_NULL_HANDLE;
    }
    return XR_SUCCESS;
}

// Factory function
std::shared_ptr<OpenXRFramework> CreateOpenXRFramework(std::shared_ptr<Platform> platform) {
    return std::make_shared<OpenXRFramework>(platform);
}