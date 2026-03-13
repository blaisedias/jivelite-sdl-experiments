#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_blendmode.h>

#include "application.h"
#include "widgets.h"
#include "touch_screen_sdl2.h"
#include "visualizer.h"
#include "logging.h"
#include "timing.h"
#include "texture_cache.h"
#include "lyrion_player.h"

#define HIDE_CURSOR_COUNT  50
#define IMAGE_FLAGS IMG_INIT_PNG

static SDL_RendererFlags render_flags = SDL_RENDERER_ACCELERATED;
static int show_cursor = 0;
static bool input_loop = true;
static volatile bool render_loop = true;
static uint32_t render_iters;
static uint32_t low_fps_count;

void sdl_render_loop(view_context* view) {
    const app_context* app_ctx = view->app;
    bool profile_fps_deviation = app_ctx->profile_fps_deviation;
    SDL_RenderClear(app_ctx->renderer);
    int vols[2] = {0, 0};
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_ShowCursor(SDL_DISABLE);

    // try to set ms_00 to immediately after return from
    // SDL_RenderPresent with vsync set.
    SDL_RenderSetVSync(app_ctx->renderer, 1);
    SDL_RenderClear(app_ctx->renderer);
    SDL_RenderPresent(app_ctx->renderer);
    SDL_RenderClear(app_ctx->renderer);
    SDL_RenderPresent(app_ctx->renderer);
    int64_t ms_00 = get_micro_seconds();
    int64_t ms_next = ms_00 + app_ctx->frame_time_micros;
    SDL_RenderSetVSync(app_ctx->renderer, app_ctx->vsync);

    while (__atomic_load_n(&render_loop, __ATOMIC_ACQUIRE)) {
        int64_t ms_0 = get_micro_seconds();
        SDL_PumpEvents();
        int64_t ms_pe = get_micro_seconds();
        tcache_render_prep(app_ctx->renderer);
//        tcache_flush_textures(app_ctx->renderer);
//        tcache_resolve_textures(app_ctx->renderer);
        int64_t ms_1 = get_micro_seconds();
        visualizer_vumeter(vols);
        int64_t ms_2 = get_micro_seconds();
        SDL_RenderClear(app_ctx->renderer);

        int64_t ms_3 = get_micro_seconds();

        for(widget* widget=view->list->head.next; widget != NULL; widget=widget->next) {
            if (!widget->hidden) {
                widget->render(widget);
            }
        }
        int64_t ms_4 = get_micro_seconds();

        int64_t sleeptime = 0;
        if (app_ctx->vsync == 0) {
            sleeptime = ms_next - 1000 - get_micro_seconds();
            sleep_micro_seconds(sleeptime);
        }
        int64_t ms_5 = get_micro_seconds();
        SDL_RenderPresent(app_ctx->renderer);
        int64_t ms_6 = get_micro_seconds();
//        profile_printf("fps=%02lu t=%06lu v=%06lu rt=%06lu wr=%06lu rtwr= rp=%06lu\n",
        int64_t fps = 1000000/(ms_6 - ms_00);
//        if ( !profile_fps_deviation || (fps < 59 || fps > 61)) {
        if ( !profile_fps_deviation || (fps < 59)) {
            ++low_fps_count;
            if (app_ctx->vsync == 0) {
                profile_printf("fps=%03ld t=%06ld pr=%06ld v=%06ld rc=%06ld wr=%06ld s=%06ld rp=%06ld pe=%06ld off= %06ld rp+s=%06ld xs=%06ld \n",
                    fps,
                    ms_6 - ms_00, //t
                    ms_1 - ms_pe, //pr
                    ms_2 - ms_1, //v
                    ms_3 - ms_2, //rc
                    ms_4 - ms_3, //wr
                    ms_5 - ms_4, //s
                    ms_6 - ms_5, //rp
                    ms_pe - ms_0, //pe
                    ms_6 - ms_next,
                    ms_6 - ms_4, //rp +s
                    ms_5 - ms_4 - sleeptime
                   );
            } else {
                profile_printf("fps=%03ld t=%06ld pr=%06ld v=%06ld rc=%06ld wr=%06ld rp=%06ld pe=%06ld ct=%06ld\n",
                    fps,
                    ms_6 - ms_00, //t
                    ms_1 - ms_pe, //pr
                    ms_2 - ms_1, //v
                    ms_3 - ms_2, //rc
                    ms_4 - ms_3, //wr
                    ms_6 - ms_5, //rp
                    ms_pe - ms_0, //pe
                    ms_1 - ms_0 + ms_2 - ms_1 + ms_3 - ms_2 + ms_4 - ms_3 + ms_6 - ms_5 +  ms_pe - ms_0
                   );
            }
        }
        ++render_iters;
        ms_00 = ms_6;
        ms_next += app_ctx->frame_time_micros;

//        SDL_ShowCursor(show_cursor != 0 ? SDL_ENABLE : SDL_DISABLE);
    }
    profile_printf("low_fps_count=%u/%u %f\n", low_fps_count, render_iters, (float)low_fps_count*100/render_iters);
    debug_printf("*** render loop end ****\n");
}

bool app_initialize(app_context* app_ctx, const char* window_title) {
    if (app_ctx->vsync) {
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
                    app_ctx->refresh_rate = dm.refresh_rate;
                    app_ctx->frame_time_millis = 1000/dm.refresh_rate;
                    app_ctx->frame_time_micros = 1000000/dm.refresh_rate;
                    printf("Display:%d fmt=%x, w=%d, h=%d, refresh rate:%d %d milliSeconds %d microSeconds\n",
                            i_display,
                            dm.format, dm.w, dm.h,
                            dm.refresh_rate,
                            app_ctx->frame_time_millis,
                            app_ctx->frame_time_micros);
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
                if (app_ctx->fullscreen) {
                    app_ctx->window = SDL_CreateWindow(window_title,
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           0, 0,
                           SDL_WINDOW_FULLSCREEN_DESKTOP);
                } else {
                    app_ctx->window = SDL_CreateWindow(window_title,
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            app_ctx->screen_width, app_ctx->screen_height,
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
                app_ctx->window = SDL_CreateWindow(window_title,
                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       0, 0,
                       SDL_WINDOW_FULLSCREEN_DESKTOP);
               break;
           case SDL_SYSWM_RISCOS:
               // puts("SDL_SYSWM_RISCOS");
               break;
        }
    }

    if (!app_ctx->window) {
        error_printf("creating window: %s\n", SDL_GetError());
        return true;
    }
//    app_ctx->renderer = SDL_CreateRenderer(app_ctx->window, -1, 0);
    app_ctx->renderer = SDL_CreateRenderer(app_ctx->window, -1, render_flags);
    if (!app_ctx->renderer) {
        error_printf("creating renderer: %s\n", SDL_GetError());
        return true;
    }
    tcache_set_renderer_tid(SDL_GetThreadID(NULL));
    SDL_GetWindowSize(app_ctx->window, &app_ctx->screen_width, &app_ctx->screen_height);
    app_ctx->pixelFormat = SDL_GetWindowPixelFormat(app_ctx->window);
    app_ctx->bytes_per_pixel = SDL_BYTESPERPIXEL(app_ctx->pixelFormat);

//    srand((unsigned)time(NULL));
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(app_ctx->renderer, app_ctx->screen_width, app_ctx->screen_height);

    printf("pixelFormat: 0x%x %u bytes/pixel ", app_ctx->pixelFormat, app_ctx->bytes_per_pixel);
    switch(app_ctx->pixelFormat) {
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
    return false;
}

void app_cleanup(app_context* app_ctx, int exit_status) {
//    TTF_CloseFont(app->text_font);
    SDL_DestroyRenderer(app_ctx->renderer);
    SDL_DestroyWindow(app_ctx->window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    exit(exit_status);
}

void print_app_runtime_info(app_context* app_ctx) {
    SDL_RendererInfo info;
    if (0 == SDL_GetRendererInfo(app_ctx->renderer, &info)) {
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
    printf("display:%dx%d Orientation:%f,  max seconds:%u performance freq:%lu\n",
           app_ctx->screen_width,
           app_ctx->screen_height,
           app_ctx->orientation,
           app_ctx->max_secs,
           SDL_GetPerformanceFrequency());
}

void sdl_input_loop(view_context* view) {
    const app_context* app_ctx = view->app;
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    bool ignore_SDL_FINGER = 0 == start_touch_screen_event_generator(NULL);
    uint32_t iters=0;
    ((app_context *)app_ctx)->player = open_local_player(app_ctx->lms);
    player_transient_state pts;

    while (input_loop) {
        if (iters % 5 == 0) {
            if (poll_player(app_ctx->player, &pts)) {
                bool can_seek = true;
                {
                    player_value pvalue;
                    switch(get_player_value(app_ctx->player, &pvalue, "CAN_SEEK")) {
                            case PFV_NONE:
                                error_printf("got nothing for player value CAN_SEEK\n");
                                break;
                            case PFV_INT:
                                debug_printf("got int %d for player CAN_SEEK\n", pvalue.integer);
                                can_seek = pvalue.integer;
                                break;
                            case PFV_STRINGPTR:
                                error_printf("got string %s for player value CAN_SEEK\n", pvalue.strptr);
                                break;
                    }
                }

                for(widget* t = view->list->tail.prev; t != NULL; t = t->prev) {
                    if (t->player_value_key) {
                        player_value pvalue;
                        switch(get_player_value(app_ctx->player, &pvalue, t->player_value_key)) {
                            case PFV_NONE:
                                error_printf("got nothing for player value %s\n", t->player_value_key);
                                break;
                            case PFV_INT:
                                debug_printf("got int %d for player value %s\n", pvalue.integer, t->player_value_key);
                                if (t->type == WIDGET_MULTISTATE_BUTTON) {
                                    widget_multistate_button_set_state(t, pvalue.integer);
                                } else if (t->type == WIDGET_SLIDER) {
                                    widget_slider_set_value(t, pvalue.integer);
                                }
                                break;
                            case PFV_STRINGPTR:
                                error_printf("got string %s for player value %s\n", pvalue.strptr, t->player_value_key);
                                break;
                        }
                    }
                    if (t->player_range_value_key) {
                        player_value pvalue;
                        switch(get_player_value(app_ctx->player, &pvalue, t->player_range_value_key)) {
                            case PFV_NONE:
                                error_printf("got nothing for player range value %s\n", t->player_range_value_key);
                                break;
                            case PFV_INT:
                                debug_printf("got int %d for player range value %s\n", pvalue.integer, t->player_range_value_key);
                                if (t->type == WIDGET_SLIDER) {
                                    widget_slider_range(t, 0, pvalue.integer);
                                }
                                break;
                            case PFV_STRINGPTR:
                                error_printf("got string %s for player range value %s\n", pvalue.strptr, t->player_range_value_key);
                                break;
                        }
                    }
                    if (t->type == WIDGET_SLIDER && 0 == strcmp(t->player_value_key, "time")) {
                        widget_slider_set_interactive(t, can_seek);
                    }
                }
            }
            player_value pvalue;
            get_player_value(app_ctx->player, &pvalue, "VOLUME");
            int volume = pvalue.integer;
            get_player_value(app_ctx->player, &pvalue, "DURATION");
            int duration = pvalue.integer;
            get_player_value(app_ctx->player, &pvalue, "time");
            int elapsed = pvalue.integer;
            for(widget* t = view->list->tail.prev; t != NULL; t = t->prev) {
                if (t->player_value_key) {
                    if (0 == strcmp("VOLUME", t->player_value_key)) {
                        if (t->type == WIDGET_SLIDER) {
                            widget_slider_set_value(t, volume);
                        }
                    }
                    if (duration && 0 == strcmp("time", t->player_value_key)) {
                        if (t->type == WIDGET_SLIDER) {
                            widget_slider_set_value(t, elapsed);
                        }
                    }
                }
            }
        }
        ++iters;
        int64_t t0 = get_micro_seconds();
        SDL_Event event;
        while(SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
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
                break;
            case SDL_MOUSEMOTION:
                {
                    SDL_ShowCursor(SDL_ENABLE);
                    show_cursor = HIDE_CURSOR_COUNT;
                    SDL_Point pt = {.x=event.button.x, .y=event.button.y};
                    widget_list_react(view->list, POINTER_MOTION, &pt);
                } break;
            case SDL_MOUSEBUTTONDOWN:
                {
                    SDL_ShowCursor(SDL_ENABLE);
                    show_cursor = HIDE_CURSOR_COUNT;
                    SDL_Point pt = {.x=event.button.x, .y=event.button.y};
                    widget_list_react(view->list, POINTER_DOWN, &pt);
                } break;
            case SDL_MOUSEBUTTONUP:
                {
                    SDL_ShowCursor(SDL_ENABLE);
                    show_cursor = HIDE_CURSOR_COUNT;
                    SDL_Point pt = {.x=event.button.x, .y=event.button.y};
                    widget_list_react(view->list, POINTER_UP, &pt);
                } break;
            case SDL_FINGERMOTION:
                if (ignore_SDL_FINGER) {
                    input_printf("IGNORING SFMO: %04d, %04d\n",(int)(event.tfinger.x*app_ctx->screen_width), (int)(event.tfinger.y*app_ctx->screen_height));
                } else {
                    SDL_Point pt = { 
                        .x = (int)(event.tfinger.x*app_ctx->screen_width),
                        .y = (int)(event.tfinger.y*app_ctx->screen_height)
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
                    input_printf("IGNORING SFDN: %04d, %04d\n", (int)(event.tfinger.x*app_ctx->screen_width), (int)(event.tfinger.y*app_ctx->screen_height));
                } else {
                    SDL_Point pt = { 
                        .x = (int)(event.tfinger.x*app_ctx->screen_width),
                        .y = (int)(event.tfinger.y*app_ctx->screen_height)
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
                    input_printf("IGNORING SFUP: %04d, %04d\n", (int)(event.tfinger.x*app_ctx->screen_width), (int)(event.tfinger.y*app_ctx->screen_height));
                } else {
                    SDL_Point pt = { 
                        .x = (int)(event.tfinger.x*app_ctx->screen_width),
                        .y = (int)(event.tfinger.y*app_ctx->screen_height)
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
        if (iters >= app_ctx->max_secs*10) {
            printf("terminating: iterations=%d max_secs=%d\n",
                   iters, app_ctx->max_secs);
            __atomic_clear(&render_loop, __ATOMIC_RELEASE);
            input_loop = false;
        }
        if (show_cursor) {
            if (0 >= --show_cursor) {
                SDL_ShowCursor(SDL_DISABLE);
                show_cursor = 0;
                for(widget* widget=view->list->tail.prev; widget != NULL; widget=widget->prev) {
                    if (widget->hidden) { continue;}
                    widget->focussed = false;
                    widget_set_highlight(widget, false);
                }
            }
        }
        // close to 100 milliseconds
        if (iters % (app_ctx->cycle_secs*10) == 0) {
            for(widget* t = view->list->head.next; t != NULL; t = t->next) {
                if (t->type == WIDGET_VUMETER) {
                    widget_vumeter_select_next(t);
                }
            }
        }
        sleep_micro_seconds(t0 + 100000 - get_micro_seconds());
    }
    stop_touch_screen_event_generator();
    debug_printf("*** input loop end ****\n");
}
