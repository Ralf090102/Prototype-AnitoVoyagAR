#include <android_native_app_glue.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>

struct AndroidAppState {
    bool resumed = false;
};

static void AppHandleCmd(struct android_app *app, int32_t cmd) {
    auto *app_state = reinterpret_cast<AndroidAppState *>(app->userData);
    switch (cmd) {
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
        case APP_CMD_INIT_WINDOW: {
            spdlog::info("APP_CMD_INIT_WINDOW surfaceCreated()");
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            spdlog::info("APP_CMD_TERM_WINDOW surfaceDestroyed()");
            break;
        }
        default: {
            spdlog::info("Unknown Command");
            break;
        }
    }
}

void android_main(struct android_app *app) {
    try {
        auto android_logger = spdlog::android_logger_mt("android", "spdlog-android");
        android_logger->set_level(spdlog::level::info);
        spdlog::set_default_logger(android_logger);
        android_logger->set_pattern("%n: %v");

        JNIEnv *env;
        app->activity->vm->AttachCurrentThread(&env, nullptr);

        AndroidAppState app_state = {};

        app->userData = &app_state;
        app->onAppCmd = AppHandleCmd;




    } catch (const std::exception &ex) {
        spdlog::error(ex.what());
    } catch (...) {
        spdlog::error("Unknown Error");
    }
}