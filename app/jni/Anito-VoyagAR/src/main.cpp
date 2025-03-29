#include <android_native_app_glue.h>
#include <android/native_activity.h>
#include <android/permission_manager.h>
#include <android/log.h>

// Platform and OpenXR includes
#include "Platform/platform_data.hpp"
#include "Platform/platform.hpp"
#include "OpenXRFramework.hpp"
#include "VulkanGraphicsPlugin.hpp"

// Vulkan includes
#include "VulkanRenderingContext.hpp"
#include "../../Vulkan/include/lib/VulkanUtils.hpp"

// AR components
#include "AR/ARCameraTextureManager.hpp"
#include "AR/ARTrackingManager.hpp"
#include "AR/ARDepthTextureManager.hpp"
#include "AR/ARBackgroundPipeline.hpp"
#include "AR/AROcclusionPipeline.hpp"

// Utilities
#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>
#include <memory>
#include <chrono>

#define LOG_TAG "AnitoVoyagAR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global variables for JNI callbacks
JavaVM* g_JavaVM = nullptr;
jobject g_ActivityObject = nullptr;

struct AndroidAppState {
    bool resumed = false;
    bool camera_permission_granted = false;

    // OpenXR & Vulkan components
    std::shared_ptr<OpenXRFramework> xr_framework;
    std::shared_ptr<vulkan::VulkanRenderingContext> rendering_context;

    // AR components
    std::shared_ptr<ARCameraTextureManager> camera_texture_manager;
    std::shared_ptr<ARTrackingManager> tracking_manager;
    std::shared_ptr<ARDepthTextureManager> depth_texture_manager;
    std::shared_ptr<ARBackgroundPipeline> background_pipeline;
    std::shared_ptr<AROcclusionPipeline> occlusion_pipeline;

    // JNI references for ARCore integration
    jclass ar_core_helper_class = nullptr;
    jobject ar_core_helper_object = nullptr;
    jmethodID get_camera_frame_method = nullptr;
    jmethodID get_camera_pose_method = nullptr;
    jmethodID get_depth_image_method = nullptr;
    jmethodID get_tracking_state_method = nullptr;
    jmethodID get_camera_width_method = nullptr;
    jmethodID get_camera_height_method = nullptr;
    jmethodID get_depth_width_method = nullptr;
    jmethodID get_depth_height_method = nullptr;
};

// Forward declarations
extern "C" JNIEXPORT void JNICALL Java_org_dlsugamelab_AnitoVoyagARMobile_VoyagARMainActivity_nativeOnCameraPermissionGranted(JNIEnv *env, jobject obj);
void initARCoreJNIIntegration(JNIEnv *env, AndroidAppState *state);
void updateARComponents(AndroidAppState *state, JNIEnv *env);

// Method to call updateARCore on the Java side
void CallJavaUpdateARCore() {
    JNIEnv* env;
    bool attached = false;

    // Get the JNI environment
    jint result = g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        if (g_JavaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("Failed to attach thread to JavaVM");
            return;
        }
        attached = true;
    } else if (result != JNI_OK) {
        LOGE("Failed to get JNI environment");
        return;
    }

    // Find the VoyagARMainActivity class
    jclass activityClass = env->GetObjectClass(g_ActivityObject);
    if (activityClass == nullptr) {
        LOGE("Failed to find VoyagARMainActivity class");
        if (attached) g_JavaVM->DetachCurrentThread();
        return;
    }

    // Find the updateARCore method
    jmethodID methodID = env->GetMethodID(activityClass, "updateARCore", "()V");
    if (methodID == nullptr) {
        LOGE("Failed to find updateARCore method");
        if (attached) g_JavaVM->DetachCurrentThread();
        return;
    }

    // Call the method
    env->CallVoidMethod(g_ActivityObject, methodID);

    // Detach the thread if it was attached
    if (attached) {
        g_JavaVM->DetachCurrentThread();
    }
}

static void AppHandleCmd(struct android_app *app, int32_t cmd) {
    auto *app_state = reinterpret_cast<AndroidAppState *>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            spdlog::info("APP_CMD_INIT_WINDOW surfaceCreated()");
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            spdlog::info("APP_CMD_TERM_WINDOW surfaceDestroyed()");
            break;
        }
        case APP_CMD_START: {
            spdlog::info("APP_CMD_START onStart()");
            break;
        }
        case APP_CMD_RESUME: {
            spdlog::info("APP_CMD_RESUME onResume()");
            app_state->resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            spdlog::info("APP_CMD_PAUSE onPause()");
            app_state->resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            spdlog::info("APP_CMD_STOP onStop()");
            break;
        }
        case APP_CMD_DESTROY: {
            spdlog::info("APP_CMD_DESTROY onDestroy()");
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            spdlog::info("Gained focus");
            break;
        }
        case APP_CMD_LOST_FOCUS: {
            spdlog::info("Lost focus");
            break;
        }
        default: {
            spdlog::info("Unknown Command");
            break;
        }
    }
}

extern "C" JNIEXPORT void JNICALL Java_org_dlsugamelab_AnitoVoyagARMobile_VoyagARMainActivity_nativeOnCameraPermissionGranted(JNIEnv *env, jobject obj) {
    spdlog::info("Native: Camera permission granted!");

    // Store the Activity object for later use
    if (g_ActivityObject == nullptr) {
        g_ActivityObject = env->NewGlobalRef(obj);
    }

    // We need to access the global app state to update the permission flag
    // This is just a placeholder since we don't have direct access to app_state here
    // In a real implementation, you'd use a global pointer or callback system
    // For now, we'll just log the event
    LOGI("Camera permission granted callback received");
}

void initARCoreJNIIntegration(JNIEnv *env, AndroidAppState *state) {
    // Find the ARCoreHelper class
    jclass clazz = env->FindClass("org/dlsugamelab/AnitoVoyagARMobile$ARCoreHelper");
    if (clazz == nullptr) {
        LOGE("Failed to find ARCoreHelper class");
        return;
    }

    // Create a global reference (persists across JNI calls)
    state->ar_core_helper_class = (jclass)env->NewGlobalRef(clazz);

    // Find the constructor
    jmethodID constructor = env->GetMethodID(state->ar_core_helper_class, "<init>", "()V");
    if (constructor == nullptr) {
        LOGE("Failed to find ARCoreHelper constructor");
        return;
    }

    // Create an instance of the ARCoreHelper
    jobject helper_obj = env->NewObject(state->ar_core_helper_class, constructor);
    state->ar_core_helper_object = env->NewGlobalRef(helper_obj);

    // Get method IDs for the ARCore helper methods
    state->get_camera_frame_method = env->GetMethodID(state->ar_core_helper_class, "getCameraFrame", "()[B");
    state->get_camera_pose_method = env->GetMethodID(state->ar_core_helper_class, "getCameraPose", "()[F");
    state->get_depth_image_method = env->GetMethodID(state->ar_core_helper_class, "getDepthImage", "()[B");
    state->get_tracking_state_method = env->GetMethodID(state->ar_core_helper_class, "getTrackingState", "()Z");
    state->get_camera_width_method = env->GetMethodID(state->ar_core_helper_class, "getCameraWidth", "()I");
    state->get_camera_height_method = env->GetMethodID(state->ar_core_helper_class, "getCameraHeight", "()I");
    state->get_depth_width_method = env->GetMethodID(state->ar_core_helper_class, "getDepthWidth", "()I");
    state->get_depth_height_method = env->GetMethodID(state->ar_core_helper_class, "getDepthHeight", "()I");

    // Check if all methods were found
    if (!state->get_camera_frame_method || !state->get_camera_pose_method ||
        !state->get_depth_image_method || !state->get_tracking_state_method ||
        !state->get_camera_width_method || !state->get_camera_height_method ||
        !state->get_depth_width_method || !state->get_depth_height_method) {
        LOGE("Failed to find one or more ARCoreHelper methods");
    }

    LOGI("ARCore JNI integration initialized");
}

void updateARComponents(AndroidAppState *state, JNIEnv *env) {
    if (!state->ar_core_helper_object) {
        LOGE("ARCore helper not initialized");
        return;
    }

    // Get tracking state
    jboolean tracking = env->CallBooleanMethod(state->ar_core_helper_object, state->get_tracking_state_method);
    state->tracking_manager->SetTrackingState(tracking);

    if (!tracking) {
        // If not tracking, no need to process the rest
        return;
    }

    // Get camera pose
    jfloatArray pose_array = (jfloatArray)env->CallObjectMethod(
            state->ar_core_helper_object, state->get_camera_pose_method);

    if (pose_array) {
        jfloat *pose_data = env->GetFloatArrayElements(pose_array, nullptr);
        state->tracking_manager->UpdateCameraPose(pose_data);
        env->ReleaseFloatArrayElements(pose_array, pose_data, JNI_ABORT);
        env->DeleteLocalRef(pose_array);
    }

    // Get camera image dimensions
    jint camera_width = env->CallIntMethod(state->ar_core_helper_object, state->get_camera_width_method);
    jint camera_height = env->CallIntMethod(state->ar_core_helper_object, state->get_camera_height_method);

    // Get camera frame
    jbyteArray frame_array = (jbyteArray)env->CallObjectMethod(
            state->ar_core_helper_object, state->get_camera_frame_method);

    if (frame_array && camera_width > 0 && camera_height > 0) {
        jbyte *frame_data = env->GetByteArrayElements(frame_array, nullptr);
        jsize frame_size = env->GetArrayLength(frame_array);

        state->camera_texture_manager->UpdateCameraTexture(
                reinterpret_cast<uint8_t*>(frame_data), camera_width, camera_height, 0); // 0 = RGBA format

        env->ReleaseByteArrayElements(frame_array, frame_data, JNI_ABORT);
        env->DeleteLocalRef(frame_array);
    }

    // Get depth image dimensions
    jint depth_width = env->CallIntMethod(state->ar_core_helper_object, state->get_depth_width_method);
    jint depth_height = env->CallIntMethod(state->ar_core_helper_object, state->get_depth_height_method);

    // Get depth image if available
    jbyteArray depth_array = (jbyteArray)env->CallObjectMethod(
            state->ar_core_helper_object, state->get_depth_image_method);

    if (depth_array && depth_width > 0 && depth_height > 0) {
        jbyte *depth_data = env->GetByteArrayElements(depth_array, nullptr);
        jsize depth_size = env->GetArrayLength(depth_array);

        state->depth_texture_manager->UpdateDepthTexture(
                reinterpret_cast<uint8_t*>(depth_data), depth_width, depth_height);

        env->ReleaseByteArrayElements(depth_array, depth_data, JNI_ABORT);
        env->DeleteLocalRef(depth_array);
    }

    // Update frame timing
    state->tracking_manager->UpdateFrameTime();
}

void android_main(struct android_app *app) {
    try {
        // Setup logging
        auto android_logger = spdlog::android_logger_mt("android", "spdlog-android");
        android_logger->set_level(spdlog::level::info);
        spdlog::set_default_logger(android_logger);

        // Setup JNI
        JNIEnv *env;
        app->activity->vm->AttachCurrentThread(&env, nullptr);

        // Store JavaVM globally for callbacks
        g_JavaVM = app->activity->vm;

        // Initialize app state
        AndroidAppState app_state = {};
        app->userData = &app_state;
        app->onAppCmd = AppHandleCmd;

        // Create platform data for OpenXR
        std::shared_ptr<PlatformData> platform_data = std::make_shared<PlatformData>();
        platform_data->application_vm = app->activity->vm;
        platform_data->application_activity = app->activity->clazz;

        // Create OpenXR framework
        app_state.xr_framework = CreateOpenXRFramework(CreatePlatform(platform_data));

        // Initialize OpenXR
        app_state.xr_framework->CreateInstance();
        app_state.xr_framework->CreateDebugMessenger();
        app_state.xr_framework->GetInstanceProperties();
        app_state.xr_framework->GetSystemID();

        // Get view configuration for AR
        app_state.xr_framework->GetViewConfigurationViews();
        app_state.xr_framework->GetEnvironmentBlendModes();

        // Create XR session - this also initializes the Vulkan device
        app_state.xr_framework->CreateSession();

        // Initialize input actions
        app_state.xr_framework->CreateActionSet();
        app_state.xr_framework->SuggestBindings();
        app_state.xr_framework->AttachActionSet();

        // Create reference space for AR (typically STAGE for AR)
        app_state.xr_framework->CreateReferenceSpace();

        // Create swapchains for rendering
        app_state.xr_framework->CreateSwapchains();

        // Get the initialized Vulkan rendering context
        // In a real implementation, you would need to extract this from the GraphicsPlugin
        // This is a placeholder - the actual mechanism will depend on your GraphicsPlugin implementation
        // app_state.rendering_context = extractRenderingContextFromGraphicsPlugin();

        // Create AR components
        app_state.tracking_manager = std::make_shared<ARTrackingManager>();
        app_state.camera_texture_manager = std::make_shared<ARCameraTextureManager>(app_state.rendering_context);
        app_state.depth_texture_manager = std::make_shared<ARDepthTextureManager>(app_state.rendering_context);
        app_state.background_pipeline = std::make_shared<ARBackgroundPipeline>(
                app_state.rendering_context, app_state.camera_texture_manager);
        app_state.occlusion_pipeline = std::make_shared<AROcclusionPipeline>(
                app_state.rendering_context, app_state.depth_texture_manager);

        // Initialize ARCore integration
        initARCoreJNIIntegration(env, &app_state);

        // Main loop
        while (app->destroyRequested == 0) {
            // Process Android events
            for (;;) {
                int events;
                struct android_poll_source *source;
                const int kTimeoutMilliseconds =
                        (!app_state.resumed && !app_state.xr_framework->IsSessionRunning() && app->destroyRequested == 0) ? -1 : 0;
                if (ALooper_pollOnce(kTimeoutMilliseconds, nullptr, &events, (void **) &source) < 0) {
                    break;
                }
                if (source != nullptr) {
                    source->process(app, source);
                }
            }

            // Call Java to update ARCore
            CallJavaUpdateARCore();

            // Process OpenXR events
            app_state.xr_framework->PollEvents();
            if (!app_state.xr_framework->IsSessionRunning()) {
                continue;
            }

            // Update AR data from ARCore
            updateARComponents(&app_state, env);

            // Poll OpenXR actions
            app_state.xr_framework->PollEvents();

            // Render frame
            app_state.xr_framework->RenderFrame();
        }

        // Cleanup
        if (app_state.ar_core_helper_object) {
            env->DeleteGlobalRef(app_state.ar_core_helper_object);
        }
        if (app_state.ar_core_helper_class) {
            env->DeleteGlobalRef(app_state.ar_core_helper_class);
        }
        if (g_ActivityObject) {
            env->DeleteGlobalRef(g_ActivityObject);
            g_ActivityObject = nullptr;
        }

        app->activity->vm->DetachCurrentThread();

    } catch (const std::exception &ex) {
        spdlog::error(ex.what());
    } catch (...) {
        spdlog::error("Unknown Error");
    }
}
