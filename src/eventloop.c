#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_blendmode.h>

#include "widgets.h"
#include "touch_screen_sdl2.h"
#include "visualizer.h"
#include "logging.h"
#include "timer.h"
#include "texture_cache.h"

#define HIDE_CURSOR_COUNT  300
#define IMAGE_FLAGS IMG_INIT_PNG

static SDL_RendererFlags render_flags = SDL_RENDERER_ACCELERATED;
static int hide_cursor_count = 0;
static bool input_loop = true;
static bool render_loop = true;

void sdl_render_loop(view_context* view) {
    const app_context* app_context = view->app;
    SDL_RenderClear(app_context->renderer);
    int vols[2] = {0, 0};
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_ShowCursor(SDL_DISABLE);
    uint64_t pfreq_micro_s = SDL_GetPerformanceFrequency();
    pfreq_micro_s /= 1000000;
    Uint32 iters = 0;

    while (render_loop) {
        tcache_resolve_textures(app_context->renderer);
        uint64_t ms_0 = getMicros();
        uint64_t pc_0 = SDL_GetPerformanceCounter();
        if (hide_cursor_count) {
            if (0 >= --hide_cursor_count) {
                SDL_ShowCursor(SDL_DISABLE);
                hide_cursor_count = 0;
                    for(widget* widget=view->list->tail.prev; widget != NULL; widget=widget->prev) {
                        if (widget->hidden) { continue;}
                        widget->focussed = false;
                        widget->highlight = false;
                    }
            }
        }

        visualizer_vumeter(vols);
        Uint64 pc_1 = SDL_GetPerformanceCounter();
        uint64_t ms_1 = getMicros();
        SDL_RenderClear(app_context->renderer);

        Uint64 pc_2 = SDL_GetPerformanceCounter();
        uint64_t ms_2 = getMicros();

        for(widget* widget=view->list->head.next; widget != NULL; widget=widget->next) {
            if (!widget->hidden) {
                widget->render(widget);
            }
        }

        SDL_RenderPresent(app_context->renderer);
        Uint64 pc_3 = SDL_GetPerformanceCounter();
        uint64_t ms_3 = getMicros();
        frame_perf_printf("\r%05.2f ms, %05.2f ms  %lu %lu %lu %lu %lu %lu                     ",
               (1000.0 *(pc_2-pc_1))/SDL_GetPerformanceFrequency(),
               (1000.0 *(pc_3-pc_1))/SDL_GetPerformanceFrequency(),
               ms_2 - ms_1, ms_3 - ms_1, ms_3 - ms_0,
               (pc_2-pc_1)/pfreq_micro_s,
               (pc_3-pc_1)/pfreq_micro_s,
               (pc_3-pc_0)/pfreq_micro_s
               );
        ++iters;

        if (app_context->delay) {
            SDL_Delay(app_context->delay);
        }
        if (iters >= app_context->max_iters) {
            debug_printf("*** iters=%d == max_iters%d ****\n");
            render_loop = false;
            input_loop = false;
        }
        if (iters % app_context->cycle_iters == 0) {
            for(widget* t = view->list->head.next; t != NULL; t = t->next) {
                if (t->type == WIDGET_VUMETER) {
                    widget_vumeter_select_next(t);
                }
            }
        }
    }
    debug_printf("*** render loop end ****\n");
}

#if 0
#if 1
#define CALIBRATION_FRAME_COUNT  360
static int calibrate_delay(app_context* app_context) {
    SDL_Texture* texture=IMG_LoadTexture(app_context->renderer,"./images/512-color-spiral-1542940319F0y.png");
    SDL_Rect src_rect = {0,0,0,0};
    SDL_QueryTexture(texture, NULL, NULL, &src_rect.w, &src_rect.h);
    float scaleh = (float)app_context->screen_width/src_rect.w;
    float scalev = (float)app_context->screen_height/src_rect.h;
    float scale = scaleh > scalev ? scaleh : scalev;
    SDL_Rect dst_rect = {0,0,src_rect.w*scale,src_rect.h*scale};
    dst_rect.x = (app_context->screen_width - dst_rect.w)/2;
    dst_rect.y = (app_context->screen_height - dst_rect.h)/2;

    float rotation = 0.0;    
    SDL_RenderClear(app_context->renderer);
    SDL_RenderPresent(app_context->renderer);
    uint64_t micros = getMicros();
    for (int i = 0; i < CALIBRATION_FRAME_COUNT; ++i) {
        SDL_RenderClear(app_context->renderer);
        SDL_RenderCopyEx(app_context->renderer, texture, &src_rect,
            &dst_rect, rotation, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(app_context->renderer);
        rotation += 1;
    }
    // milliseconds for 1000 frames
    micros = getMicros() - micros;
    // avg milliseconds per frame
    micros /= CALIBRATION_FRAME_COUNT;
    printf("Calibration for vsync: milliSeconds/frame=%f fps=%f\n",
            (float)micros/1000, (float)1000000/micros);
    SDL_DestroyTexture(texture);
    return micros/1000;
#undef CALIBRATE_FRAME_COUNT
}

#else
int calibrate_delay(SDL_Renderer* renderer) {
#define NSAMPLES 120
    Uint64 pc[NSAMPLES + 1];
    Uint32 ticks[NSAMPLES + 1];
//    SDL_RenderClear(renderer);
    for (int i = 0; i < sizeof(pc)/sizeof(*pc); ++i) {
        SDL_RenderPresent(renderer);
        pc[i] = SDL_GetPerformanceCounter();
        ticks[i] = SDL_GetTicks();
        SDL_Delay(1);
    }
    float pfsum = 0.0;
    float ticksum = 0.0;
    for (int i = sizeof(pc)/sizeof(*pc)-1; i> 0; --i) {
        pc[i] -= pc[i-1];
        pfsum += pc[i];
        ticks[i] -= ticks[i-1];
        ticksum += ticks[i];
    }
    printf("Calibration: Ticks: fps=%5.2f, sum=%f avg=%f\n", 1000.0/(ticksum/NSAMPLES), ticksum, ticksum/NSAMPLES);
    printf("             PC   : fps=%5.2f, sum=%f avg=%f t=%f ms\n", SDL_GetPerformanceFrequency()/(pfsum/NSAMPLES), pfsum, pfsum/NSAMPLES, ((pfsum/NSAMPLES)/SDL_GetPerformanceFrequency())*1000);
    printf("    *******: Ticks:%d PFC:%d\n",
            (int)(ticksum/NSAMPLES),
            (int)((pfsum/NSAMPLES)/SDL_GetPerformanceFrequency()));
    return (int)(ticksum/NSAMPLES);
}
#endif
#endif

bool app_initialize(app_context* app_context, const char* window_title) {
    if (app_context->vsync) {
        render_flags |=  SDL_RENDERER_PRESENTVSYNC;
    }

    if (SDL_Init(SDL_INIT_EVERYTHING)) {
        error_printf("initializing SDL: %s\n", SDL_GetError());
        return true;
    }

    int img_init = IMG_Init(IMAGE_FLAGS);
    if ((img_init & IMAGE_FLAGS) != IMAGE_FLAGS) {
        error_printf("initializing SDL_image: %s\n", IMG_GetError());
        return true;
    }

    if (TTF_Init()) {
        error_printf("initializing SDL_ttf: %s\n", IMG_GetError());
        return true;
    }

    int num_displays = SDL_GetNumVideoDisplays();
    for (int i_display = 0; i_display < num_displays; ++ i_display) {
            SDL_DisplayMode dm;
            if (0 == SDL_GetCurrentDisplayMode(i_display, &dm)) {
                // TODO: handle multiple displays?
                if (i_display == 0) {
                    app_context->refresh_rate = dm.refresh_rate;
                    app_context->frame_time_millis = 1000/dm.refresh_rate;
                    app_context->frame_time_micros = 1000000/dm.refresh_rate;
                    printf("Display:%d fmt=%x, w=%d, h=%d, refresh rate:%d %d milliSeconds %d microSeconds\n",
                            i_display,
                            dm.format, dm.w, dm.h,
                            dm.refresh_rate,
                            app_context->frame_time_millis,
                            app_context->frame_time_micros);
                }
            } 
    }

    SDL_Window *window;

    sleep(1);
    window =  SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);

    SDL_SysWMinfo swmi;
    if (SDL_GetWindowWMInfo(window, &swmi)) {
        SDL_DestroyWindow(window);
        switch(swmi.subsystem) {
           case SDL_SYSWM_UNKNOWN:
               // puts("SDL_SYSWM_UNKNOWN");
               break;
           case SDL_SYSWM_WINDOWS:
               // puts("SDL_SYSWM_WINDOWS");
               break;
           case SDL_SYSWM_X11:
               // puts("SDL_SYSWM_X11");
               break;
           case SDL_SYSWM_DIRECTFB:
               // puts("SDL_SYSWM_DIRECTFB");
               break;
           case SDL_SYSWM_COCOA:
               // puts("SDL_SYSWM_COCOA");
               break;
           case SDL_SYSWM_UIKIT:
               // puts("SDL_SYSWM_UIKIT");
               break;
           case SDL_SYSWM_WAYLAND:
/*               
               if (orientation == 90.0 || orientation ==270) {
                   int tmp = screen_width;
                   screen_width = screen_height;
                   screen_height = tmp;
               }
*/
                // puts("SDL_SYSWM_WAYLAND");
                if (app_context->fullscreen) {
                    app_context->window = SDL_CreateWindow(window_title,
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           0, 0,
                           SDL_WINDOW_FULLSCREEN_DESKTOP);
                } else {
                    app_context->window = SDL_CreateWindow(window_title,
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            app_context->screen_width, app_context->screen_height,
                            0);
                }
               break;
           case SDL_SYSWM_MIR:
               // puts("SDL_SYSWM_MIR");
               break;
           case SDL_SYSWM_WINRT:
               // puts("SDL_SYSWM_WINRT");
               break;
           case SDL_SYSWM_ANDROID:
               // puts("SDL_SYSWM_ANDROID");
               break;
           case SDL_SYSWM_VIVANTE:
               // puts("SDL_SYSWM_VIVANTE");
               break;
           case SDL_SYSWM_OS2:
               // puts("SDL_SYSWM_OS2");
               break;
           case SDL_SYSWM_HAIKU:
               // puts("SDL_SYSWM_HAIKU");
               break;
           case SDL_SYSWM_KMSDRM:
                puts("SDL_SYSWM_KMSDRM");
                app_context->window = SDL_CreateWindow(window_title,
                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       0, 0,
                       SDL_WINDOW_FULLSCREEN_DESKTOP);
               break;
           case SDL_SYSWM_RISCOS:
               // puts("SDL_SYSWM_RISCOS");
               break;
        }
    }

    if (!app_context->window) {
        error_printf("creating window: %s\n", SDL_GetError());
        return true;
    }
//    app_context->renderer = SDL_CreateRenderer(app_context->window, -1, 0);
    app_context->renderer = SDL_CreateRenderer(app_context->window, -1, render_flags);
    if (!app_context->renderer) {
        error_printf("creating renderer: %s\n", SDL_GetError());
        return true;
    }
    SDL_GetWindowSize(app_context->window, &app_context->screen_width, &app_context->screen_height);
    app_context->pixelFormat = SDL_GetWindowPixelFormat(app_context->window);

//    srand((unsigned)time(NULL));
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(app_context->renderer, app_context->screen_width, app_context->screen_height);

    printf("pixeFormat: %x ", app_context->pixelFormat);
    switch(app_context->pixelFormat) {
        case SDL_PIXELFORMAT_UNKNOWN: printf("SDL_PIXELFORMAT_UNKNOWN\n"); break;
        case SDL_PIXELFORMAT_INDEX1LSB: printf("SDL_PIXELFORMAT_INDEX1LSB\n"); break;
        case SDL_PIXELFORMAT_INDEX1MSB: printf("SDL_PIXELFORMAT_INDEX1MSB\n"); break;
        case SDL_PIXELFORMAT_INDEX4LSB: printf("SDL_PIXELFORMAT_INDEX4LSB\n"); break;
        case SDL_PIXELFORMAT_INDEX4MSB: printf("SDL_PIXELFORMAT_INDEX4MSB\n"); break;
        case SDL_PIXELFORMAT_INDEX8: printf("SDL_PIXELFORMAT_INDEX8\n"); break;
        case SDL_PIXELFORMAT_RGB332: printf("SDL_PIXELFORMAT_RGB332\n"); break;
        case SDL_PIXELFORMAT_RGB444: printf("SDL_PIXELFORMAT_RGB444\n"); break;
        case SDL_PIXELFORMAT_RGB555: printf("SDL_PIXELFORMAT_RGB555\n"); break;
        case SDL_PIXELFORMAT_BGR555: printf("SDL_PIXELFORMAT_BGR555\n"); break;
        case SDL_PIXELFORMAT_ARGB4444: printf("SDL_PIXELFORMAT_ARGB4444\n"); break;
        case SDL_PIXELFORMAT_RGBA4444: printf("SDL_PIXELFORMAT_RGBA4444\n"); break;
        case SDL_PIXELFORMAT_ABGR4444: printf("SDL_PIXELFORMAT_ABGR4444\n"); break;
        case SDL_PIXELFORMAT_BGRA4444: printf("SDL_PIXELFORMAT_BGRA4444\n"); break;
        case SDL_PIXELFORMAT_ARGB1555: printf("SDL_PIXELFORMAT_ARGB1555\n"); break;
        case SDL_PIXELFORMAT_RGBA5551: printf("SDL_PIXELFORMAT_RGBA5551\n"); break;
        case SDL_PIXELFORMAT_ABGR1555: printf("SDL_PIXELFORMAT_ABGR1555\n"); break;
        case SDL_PIXELFORMAT_BGRA5551: printf("SDL_PIXELFORMAT_BGRA5551\n"); break;
        case SDL_PIXELFORMAT_RGB565: printf("SDL_PIXELFORMAT_RGB565\n"); break;
        case SDL_PIXELFORMAT_BGR565: printf("SDL_PIXELFORMAT_BGR565\n"); break;
        case SDL_PIXELFORMAT_RGB24: printf("SDL_PIXELFORMAT_RGB24\n"); break;
        case SDL_PIXELFORMAT_BGR24: printf("SDL_PIXELFORMAT_BGR24\n"); break;
        case SDL_PIXELFORMAT_RGB888: printf("SDL_PIXELFORMAT_RGB888\n"); break;
        case SDL_PIXELFORMAT_RGBX8888: printf("SDL_PIXELFORMAT_RGBX8888\n"); break;
        case SDL_PIXELFORMAT_BGR888: printf("SDL_PIXELFORMAT_BGR888\n"); break;
        case SDL_PIXELFORMAT_BGRX8888: printf("SDL_PIXELFORMAT_BGRX8888\n"); break;
        case SDL_PIXELFORMAT_ARGB8888: printf("SDL_PIXELFORMAT_ARGB8888\n"); break;
        case SDL_PIXELFORMAT_RGBA8888: printf("SDL_PIXELFORMAT_RGBA8888\n"); break;
        case SDL_PIXELFORMAT_ABGR8888: printf("SDL_PIXELFORMAT_ABGR8888\n"); break;
        case SDL_PIXELFORMAT_BGRA8888: printf("SDL_PIXELFORMAT_BGRA8888\n"); break;
        case SDL_PIXELFORMAT_ARGB2101010: printf("SDL_PIXELFORMAT_ARGB2101010\n"); break;
//        case SDL_PIXELFORMAT_RGBA32: printf("SDL_PIXELFORMAT_RGBA32\n"); break;
//        case SDL_PIXELFORMAT_ARGB32: printf("SDL_PIXELFORMAT_ARGB32\n"); break;
//        case SDL_PIXELFORMAT_BGRA32: printf("SDL_PIXELFORMAT_BGRA32\n"); break;
//        case SDL_PIXELFORMAT_ABGR32: printf("SDL_PIXELFORMAT_ABGR32\n"); break;
        case SDL_PIXELFORMAT_YV12: printf("SDL_PIXELFORMAT_YV12\n"); break;
        case SDL_PIXELFORMAT_IYUV: printf("SDL_PIXELFORMAT_IYUV\n"); break;
        case SDL_PIXELFORMAT_YUY2: printf("SDL_PIXELFORMAT_YUY2\n"); break;
        case SDL_PIXELFORMAT_UYVY: printf("SDL_PIXELFORMAT_UYVY\n"); break;
        case SDL_PIXELFORMAT_YVYU: printf("SDL_PIXELFORMAT_YVYU\n"); break;
        case SDL_PIXELFORMAT_NV12: printf("SDL_PIXELFORMAT_NV12\n"); break;
        case SDL_PIXELFORMAT_NV21: printf("SDL_PIXELFORMAT_NV21\n"); break;
    }
    /*
    if (render_flags & SDL_RENDERER_PRESENTVSYNC) {
        SDL_RenderSetVSync(app_context->renderer, 1);
        app_context->delay = calibrate_delay(app_context) - 1;
        app_context->delay = MAX(app_context->delay, 1);
    }
    */
    app_context->delay = app_context->frame_time_millis - 1;
    return false;
}

void app_cleanup(app_context* app_context, int exit_status) {
//    TTF_CloseFont(app->text_font);
    SDL_DestroyRenderer(app_context->renderer);
    SDL_DestroyWindow(app_context->window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    exit(exit_status);
}

void print_app_runtime_info(app_context* app_context) {
    SDL_RendererInfo info;
    if (0 == SDL_GetRendererInfo(app_context->renderer, &info)) {
        printf(
                "Renderer info:\n"
                "    name=%s\n"
                "    max_texture_width=%d\n"
                "    max_texture_height=%d\n",
                info.name,
                info.max_texture_width,
                info.max_texture_height
               );
    } else {
        printf("Failed to retrieve renderer information\n");
    }
    printf("display:%dx%d Orientation:%f, calibrated delay:%d millisseconds, maxiters:%u performance freq:%lu\n",
           app_context->screen_width,
           app_context->screen_height,
           app_context->orientation,
           app_context->delay, 
           app_context->max_iters,
           SDL_GetPerformanceFrequency());
}

void sdl_input_loop(view_context* view) {
    const app_context* app_context = view->app;
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    bool ignore_SDL_FINGER = 0 == start_touch_screen_event_generator(NULL);
    SDL_ShowCursor(SDL_DISABLE);

    while (input_loop) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case USEREVENT_NEXT_VISU:
            case USEREVENT_NEXT_VU:
                {
                    for(widget* t = view->list->tail.prev; t != NULL; t = t->prev) {
                        if (t->type == WIDGET_VUMETER) {
                            widget_vumeter_select_next(t);
                        }
                    }
                }break;
            case USEREVENT_PREV_VISU:
            case USEREVENT_PREV_VU:
                {
                    for(widget* t = view->list->tail.prev; t != NULL; t = t->prev) {
                        if (t->type == WIDGET_VUMETER) {
                            widget_vumeter_select_prev(t);
                        }
                    }
                }break;
            case SDL_QUIT:
                puts("");
                render_loop = false;
                input_loop = false;
//                app_cleanup(&app, EXIT_->UCCESS);
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE: 
                    puts("");
                    render_loop = false;
                    input_loop = false;
                    break;
                case SDL_SCANCODE_SPACE:
                    {
                        int m = tcache_get_texture_bytes_count();
                        printf("\n %d %fMiB\n", m, (float)m/(1024*1024));
                    }
                    break;
                case SDL_SCANCODE_T:
                    tcache_dump();
                    break;
                default:
                    break;
                }
            case SDL_MOUSEMOTION:
                {
                    SDL_ShowCursor(SDL_ENABLE);
                    hide_cursor_count = HIDE_CURSOR_COUNT;
                    SDL_Point pt = {.x=event.button.x, .y=event.button.y};
                    widget_list_react(view->list, POINTER_MOTION, &pt);
                } break;
            case SDL_MOUSEBUTTONDOWN:
                {
                    SDL_ShowCursor(SDL_ENABLE);
                    hide_cursor_count = HIDE_CURSOR_COUNT;
                    SDL_Point pt = {.x=event.button.x, .y=event.button.y};
                    widget_list_react(view->list, POINTER_DOWN, &pt);
                } break;
            case SDL_MOUSEBUTTONUP:
                {
                    SDL_ShowCursor(SDL_ENABLE);
                    hide_cursor_count = HIDE_CURSOR_COUNT;
                    SDL_Point pt = {.x=event.button.x, .y=event.button.y};
                    widget_list_react(view->list, POINTER_UP, &pt);
                } break;
            case SDL_FINGERMOTION:
                if (ignore_SDL_FINGER) {
                    input_printf("IGNORING SFMO: %04d, %04d\n",(int)(event.tfinger.x*app_context->screen_width), (int)(event.tfinger.y*app_context->screen_height));
                } else {
                    SDL_Point pt = { 
                        .x = (int)(event.tfinger.x*app_context->screen_width),
                        .y = (int)(event.tfinger.y*app_context->screen_height)
                    };
                    widget_list_react(view->list, POINTER_MOTION, &pt);
                }
                break;
            case USEREVENT_FINGERMOTION:
                {
                    SDL_Point pt = { .x = event.motion.x, .y = event.motion.y };
                    widget_list_react(view->list, POINTER_MOTION, &pt);
                } break;
            case SDL_FINGERDOWN:
                if (ignore_SDL_FINGER) {
                    input_printf("IGNORING SFDN: %04d, %04d\n", (int)(event.tfinger.x*app_context->screen_width), (int)(event.tfinger.y*app_context->screen_height));
                } else {
                    SDL_Point pt = { 
                        .x = (int)(event.tfinger.x*app_context->screen_width),
                        .y = (int)(event.tfinger.y*app_context->screen_height)
                    };
                    widget_list_react(view->list, POINTER_DOWN, &pt);
                }
                break;
            case USEREVENT_FINGERDOWN:
                {
                    SDL_Point pt = { .x = event.motion.x, .y = event.motion.y };
                    widget_list_react(view->list, POINTER_DOWN, &pt);
                } break;
            case SDL_FINGERUP:
                if (ignore_SDL_FINGER) {
                    input_printf("IGNORING SFUP: %04d, %04d\n", (int)(event.tfinger.x*app_context->screen_width), (int)(event.tfinger.y*app_context->screen_height));
                } else {
                    SDL_Point pt = { 
                        .x = (int)(event.tfinger.x*app_context->screen_width),
                        .y = (int)(event.tfinger.y*app_context->screen_height)
                    };
                    widget_list_react(view->list, POINTER_UP, &pt);
                }
                break;
            case USEREVENT_FINGERUP:
                {
                    SDL_Point pt = { .x = event.motion.x, .y = event.motion.y };
                    widget_list_react(view->list, POINTER_UP, &pt);
                } break;
            default:
                break;
            }
        }
        SDL_Delay(100);
    }
    stop_touch_screen_event_generator();
    debug_printf("*** input loop end ****\n");
}
