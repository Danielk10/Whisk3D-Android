#include <jni.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>

#include "AndroidOut.h"
#include "Renderer.h"

extern "C" {

/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (pApp->window != nullptr) {
                if (pApp->userData == nullptr) {
                    pApp->userData = new Renderer(pApp);
                } else {
                    auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                    pRenderer->onWindowInit();
                    pRenderer->onResume();
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            if (pApp->userData != nullptr) {
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pRenderer->onPause();
                pRenderer->onWindowTerm();
            }
            break;
        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
        case APP_CMD_LOST_FOCUS:
            if (pApp->userData != nullptr) {
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pRenderer->onPause();
            }
            break;
        case APP_CMD_START:
        case APP_CMD_RESUME:
        case APP_CMD_GAINED_FOCUS:
            if (pApp->userData != nullptr) {
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pRenderer->onResume();
            }
            break;
        case APP_CMD_DESTROY:
            if (pApp->userData != nullptr) {
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pApp->userData = nullptr;
                delete pRenderer;
            }
            break;
        default:
            break;
    }
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    // Can be removed, useful to ensure your code is running
    aout << "Welcome to android_main" << std::endl;

    // Register an event handler for Android events
    pApp->onAppCmd = handle_cmd;

    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter()
    // implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    // This sets up a typical game/event loop. It will run until the app is destroyed.
    do {
        // Process all pending events before running game logic.
        bool done = false;
        while (!done) {
            // 0 is non-blocking.
            int timeout = 0;
            int events;
            android_poll_source *pSource;
            int result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&pSource));
            switch (result) {
                case ALOOPER_POLL_TIMEOUT:
                    [[clang::fallthrough]];
                case ALOOPER_POLL_WAKE:
                    // No events occurred before the timeout or explicit wake. Stop checking for events.
                    done = true;
                    break;
                case ALOOPER_EVENT_ERROR:
                    aout << "ALooper_pollOnce returned an error" << std::endl;
                    break;
                case ALOOPER_POLL_CALLBACK:
                    break;
                default:
                    if (pSource) {
                        pSource->process(pApp, pSource);
                    }
            }
        }

        // Check if any user data is associated. This is assigned in handle_cmd
        if (pApp->userData) {
            auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);

            // Process game input
            pRenderer->handleInput();

            // Render a frame only if surface is valid
            if (pRenderer->canRender()) {
                pRenderer->render();
            }
        }
    } while (!pApp->destroyRequested);
}
}