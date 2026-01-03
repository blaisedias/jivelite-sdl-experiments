#include <SDL2/SDL_thread.h>
#include "touch_screen_sdl2.h"
#include "logging.h"

#ifdef   USE_TSLIB_FOR_TOUCH

static touch_screen_svc_config cfg;

#define TOUCH_EVENT_BUFFER_COUNT   1024
static SDL_Event touch_events[TOUCH_EVENT_BUFFER_COUNT];
static int ix = 0;

static void postMotion(int x, int y) {
    SDL_Event* evt = touch_events + ix;
    ix = (ix + 1) % TOUCH_EVENT_BUFFER_COUNT;
    evt->type = USEREVENT_FINGERMOTION;
    evt->motion.which = SDL_TOUCH_MOUSEID;
    evt->motion.x = x;
    evt->motion.y = y;
    SDL_PushEvent(evt);
}

static void postDown(int x, int y) {
    SDL_Event* evt = touch_events + ix;
    ix = (ix + 1) % TOUCH_EVENT_BUFFER_COUNT;
    evt->type = USEREVENT_FINGERDOWN;
    evt->motion.which = SDL_TOUCH_MOUSEID;
    evt->motion.x = x;
    evt->motion.y = y;
    SDL_PushEvent(evt);
}

static void postUp(int x, int y) {
    SDL_Event* evt = touch_events + ix;
    ix = (ix + 1) % TOUCH_EVENT_BUFFER_COUNT;
    evt->type = USEREVENT_FINGERUP;
    evt->motion.which = SDL_TOUCH_MOUSEID;
    evt->motion.x = x;
    evt->motion.y = y;
    SDL_PushEvent(evt);
}

static SDL_Thread* tseg_thrd = NULL;

int start_touch_screen_event_generator(touch_screen_svc_config* config) {
    if (getenv("TSLIB_TSDEVICE") == NULL) {
        error_printf("TSLIB_TSDEVICE environment variable not defined, not starting touch screen service\n");
        return EXIT_FAILURE;
    }
    if (tseg_thrd == NULL) {
        cfg.up = postUp;
        cfg.down = postDown;
        cfg.motion = postMotion;

        tseg_thrd = SDL_CreateThread((SDL_ThreadFunction)touch_screen_service, "TouchScreenEventGenerator", &cfg);
        printf("started touch screen thread %p\n", tseg_thrd);
    } else {
        error_printf("touch screen service is already running\n");
    }

    return tseg_thrd == NULL? EXIT_FAILURE : 0;
}

int stop_touch_screen_event_generator(void) {
    stop_touch_screen_service();
    if (tseg_thrd) {
        SDL_WaitThread(tseg_thrd, NULL);
        tseg_thrd = NULL;
    }
    return 0;
}

#else   // } USE_TSLIB_TOUCH {
int start_touch_screen_event_generator(touch_screen_svc_config* config) {
    return 0;
}

int stop_touch_screen_event_generator(void) {
    return 0;
}

#endif  // } USE_TSLIB_TOUCH 
