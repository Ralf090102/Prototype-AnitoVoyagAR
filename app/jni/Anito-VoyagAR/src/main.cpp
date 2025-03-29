#include "main.hpp"

struct AndroidAppState {
    bool resumed = false;
};

const char* CAMERA_PERMISSION = "android.permission.CAMERA";


JNIEnv* GetJNIEnv(struct android_app* app);
bool isPermissionGranted(struct android_app* app);
void requestPermission(struct android_app* app);
static void AppHandleCmd(struct android_app *app, int32_t cmd);

void android_main(struct android_app *app) {
    try {
        // Set up spdlog
        auto android_logger = spdlog::android_logger_mt("android", "spdlog-android");
        android_logger->set_level(spdlog::level::info);
        spdlog::set_default_logger(android_logger);
        android_logger->set_pattern("%v");

        JNIEnv *env;
        app->activity->vm->AttachCurrentThread(&env, nullptr);
        jobject activity = app->activity->clazz;

        AndroidAppState app_state = {};
        app->userData = &app_state;
        app->onAppCmd = AppHandleCmd;

//        OpenXRMain openxr;
//        VulkanMain vulkan;
//
//        if (!openxr.initialize()) {
//            spdlog::error("{}: Failed to initialize OpenXR. Exiting.", MAIN_TAG);
//
//            return;
//        }
//
//        if (!vulkan.initialize()) {
//            spdlog::error("{}: Failed to initialize Vulkan. Exiting.", MAIN_TAG);
//            openxr.cleanup();
//
//            return;
//        }

        bool running = true;
        while (running) {
            int events;
            struct android_poll_source* source;
            while (ALooper_pollOnce(0, nullptr, &events, (void**)&source) >= 0) {
                if (source) source->process(app, source);

                // Proper exit condition
                if (app->destroyRequested) {
                    running = false;
                    break;
                }
            }
        }

//        openxr.cleanup();
//        vulkan.cleanup();

        // Detach before exiting
        app->activity->vm->DetachCurrentThread();

    } catch (const std::exception &ex) {
        spdlog::error(ex.what());
    } catch (...) {
        spdlog::error("{}: Unknown Error", MAIN_TAG);
    }
}

static void AppHandleCmd(struct android_app *app, int32_t cmd) {
    auto *app_state = reinterpret_cast<AndroidAppState *>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            spdlog::info("{}: APP_CMD_INIT_WINDOW surfaceCreated()", MAIN_TAG);
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            spdlog::info("{}: APP_CMD_TERM_WINDOW surfaceDestroyed()", MAIN_TAG);
            break;
        }
        case APP_CMD_START: {
            spdlog::info("{}: APP_CMD_START onStart()", MAIN_TAG);
            break;
        }
        case APP_CMD_RESUME: {
            spdlog::info("{}: APP_CMD_RESUME onResume()", MAIN_TAG);
            app_state->resumed = true;

            spdlog::info("{}: App resumed. Checking permissions...", MAIN_TAG);
            if (!isPermissionGranted(app)) {
                spdlog::info("{}: Camera permission not granted. Requesting...", MAIN_TAG);
                requestPermission(app);
            } else {
                spdlog::info("{}: Camera permission already granted.", MAIN_TAG);
            }

            break;
        }
        case APP_CMD_PAUSE: {
            spdlog::info("{}: APP_CMD_PAUSE onPause()", MAIN_TAG);
            app_state->resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            spdlog::info("{}: APP_CMD_STOP onStop()", MAIN_TAG);
            break;
        }
        case APP_CMD_DESTROY: {
            spdlog::info("{}: APP_CMD_DESTROY onDestroy()", MAIN_TAG);
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            spdlog::info("{}: Gained focus", MAIN_TAG);
            break;
        }
        case APP_CMD_LOST_FOCUS: {
            spdlog::info("{}: Lost focus", MAIN_TAG);
            break;
        }
        default: {
            spdlog::info("{}: Unknown Command", MAIN_TAG);
            break;
        }
    }
}

JNIEnv* GetJNIEnv(struct android_app* app) {
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    vm->AttachCurrentThread(&env, nullptr);

    return env;
}

bool isPermissionGranted(struct android_app* app) {
    JNIEnv* env = GetJNIEnv(app);
    jclass activityClass = env->GetObjectClass(app->activity->clazz);
    jmethodID checkSelfPermissionMethod = env->GetMethodID(activityClass, "checkSelfPermission", "(Ljava/lang/String;)I");

    jstring jPermission = env->NewStringUTF(CAMERA_PERMISSION);
    jint result = env->CallIntMethod(app->activity->clazz, checkSelfPermissionMethod, jPermission);
    env->DeleteLocalRef(jPermission);
    env->DeleteLocalRef(activityClass);

    return (result == 0);
}


void requestPermission(struct android_app* app) {
    JNIEnv* env = GetJNIEnv(app);
    jclass activityClass = env->GetObjectClass(app->activity->clazz);
    jmethodID requestPermissionsMethod = env->GetMethodID(activityClass,"requestPermissions", "([Ljava/lang/String;I)V");

    jobjectArray permArray = env->NewObjectArray(1, env->FindClass("java/lang/String"), env->NewStringUTF(CAMERA_PERMISSION));
    env->CallVoidMethod(app->activity->clazz, requestPermissionsMethod, permArray, 1);
    env->DeleteLocalRef(permArray);
    env->DeleteLocalRef(activityClass);
}
