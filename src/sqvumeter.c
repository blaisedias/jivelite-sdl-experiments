/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "application.h"
#include "widgets.h"
#include "vumeter_util.h"
#include "timing.h"

#define WINDOW_TITLE "Squeezelite Visualiser"

const char* help_text=""
"\n"
" - [help, -h, --h] : print this text and exit\n"
"\n"  
" - vsync : use vertical sync when rendering each frame\n"
" - max_secs <count> : time to run before terminating, infinite if not specified\n"
" - cycle <count> : number of seconds before cycling to the next the VU Meter\n"
" - [0.0, 90.0, 180.0, 270.0] : rotation. Default is 0.0\n"
"\n"  
" - printfdefbug enable printing of debug\n"
" - printfinput  enable printing of input data\n"
" - printfvol    enable printing of volume levels\n"
" - printfload   enable printing of media load times\n"
" - printfscale  enable printing of scaling parameters and data\n"
" - printfperf   enable printing of performance metrics\n"
" - profile      enable printing of render loop performance metrics (per frame)\n"
" - profile_fps_deviation      enable printing of render loop performance metrics (per frame) when fps has deviated\n"
" - profile_texture   enable printing of render loop texture metrics\n"
" - printfjson   enable printing of json processing\n"
" - printfaction enable printing of actions\n"
" - printftcache enable printing of texture cache module\n"
" - printftcacheeject enable printing of texture cache module ejects\n"
"\n"  
" - list list the set of VU Meters and exit\n"
" - dl <path-to-object-file> : dynamically load VU meter in object file\n"
"\n"  
" - wxh <width> <height> : window width and height, only works if window manager is avialable\n"
" - fullscreen\n"
"\n"
" - json path to json file\n"
"\n"
" - vu <vumeter_name> first VU meter to display\n"
"\n"
" - showrects       : show widget draw rectangles when pointer is over them\n"
" - showinputrects  : show widget input rectangles when pointer is over them\n"
"\n"
" - peakhold <count>: number of frames for VU peak hold\n"
" - decayhold <count>: number of frames for VU decay hold - reduces needle jitter\n"
"\n"
" - texture_cache_size <count>: maximum number of texture bytes\n"
"\n"
" - lms <name>: lyrion media server network name or ip address \n"
"\n";  

const char* json_file="./npvu.json";

struct App {
    app_context  context;
//    TTF_Font *text_font;
    const Uint8 *keystate;
};

extern void sdl_render_loop(view_context* view);
extern void sdl_input_loop(view_context* view);

int main(int argc, char **argv) {
    struct App app = {
        .context = {
            .renderer = NULL,
            .window = NULL,
            .max_secs = (Uint32)-1,
            .cycle_secs = (Uint32)-1,
            .vsync = 0,
            .window_title = WINDOW_TITLE,
            .json_file = json_file,
            .dump_vu = false,
        },
//        .keystate = SDL_GetKeyboardState(NULL),
    };

    view_context view = {.app = &app.context, .list=create_widget_list(&view)};

    for(int i = 0; i < argc; ++i) {
        printf("%s ", argv[i]);
    }
    printf("\n");
    for(int i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], "max_secs")) {
            if (argc > i+1) {
                app.context.max_secs = atoi(argv[i+1]);
                i += 1;
            }
        } else if (0 == strcmp(argv[i], "cycle")) {
            if (argc > i+1) {
                app.context.cycle_secs = atoi(argv[i+1]);
                i += 1;
            }
        } else if (0 == strcmp(argv[i], "wxh")) {
            if (argc > i+2) {
                app.context.screen_width = atoi(argv[i+1]);
                app.context.screen_height = atoi(argv[i+2]);
                i += 2;
            }
        } else if (0 == strcmp(argv[i], "vsync")) {
            app.context.vsync = 1;
        } else if (0 == strcmp(argv[i], "dumpvu")) {
            app.context.dump_vu = true;
        } else if (0 == strcmp(argv[i], "0.0")) {
            app.context.orientation = 0.0;
        } else if (0 == strcmp(argv[i], "180.0")) {
            app.context.orientation = 180.0;
        } else if (0 == strcmp(argv[i], "90.0")) {
            app.context.orientation = 90.0;
        } else if (0 == strcmp(argv[i], "270.0")) {
            app.context.orientation = 270.0;
        } else if (0 == strcmp(argv[i], "printfdebug")) {
            enable_printf(DEBUG_PRINTF);
        } else if (0 == strcmp(argv[i], "printfinput")) {
            enable_printf(INPUT_PRINTF);
        } else if (0 == strcmp(argv[i], "printfvol")) {
            enable_printf(VOL_PRINTF);
        } else if (0 == strcmp(argv[i], "printfload")) {
            enable_printf(LOAD_PRINTF);
        } else if (0 == strcmp(argv[i], "printfscale")) {
            enable_printf(SCALE_PRINTF);
        } else if (0 == strcmp(argv[i], "printfperf")) {
            enable_printf(PERF_PRINTF);
        } else if (0 == strcmp(argv[i], "profile")) {
            enable_printf(PROFILE_PERF_PRINTF);
        } else if (0 == strcmp(argv[i], "profile_fps_deviation")) {
            enable_printf(PROFILE_PERF_PRINTF);
            app.context.profile_fps_deviation = true;
         } else if (0 == strcmp(argv[i], "profile_texture")) {
            enable_printf(PROFILE_TEXTURE_PERF_PRINTF);
         } else if (0 == strcmp(argv[i], "printfjson")) {
            enable_printf(JSON_PRINTF);
        } else if (0 == strcmp(argv[i], "printfaction")) {
            enable_printf(ACTION_PRINTF);
        } else if (0 == strcmp(argv[i], "printftcache")) {
            enable_printf(TEXTURE_CACHE_PRINTF);
        } else if (0 == strcmp(argv[i], "printftcacheeject")) {
            enable_printf(TEXTURE_CACHE_EJECT_PRINTF);
        } else if (0 == strcmp(argv[i], "debugrects")) {
            debug_rects = true;
        } else if (0 == strcmp(argv[i], "showrects")) {
            show_rects = true;
        } else if (0 == strcmp(argv[i], "showinputrects")) {
            show_input_rects = true;
        } else if (0 == strcmp(argv[i], "perf_level")) {
            if (argc > i+1) {
                VUMeter_set_perf_level(atoi(argv[i+1]));
                i += 1;
            }
         } else if (0 == strcmp(argv[i], "vu")) {
            if (argc > i+1) {
                i += 1;
                app.context.first_vu_meter = argv[i];
            }
        } else if (0 == strcmp(argv[i], "list") ){
            const vumeter_properties *p = VUMeter_get_props_list();
            while(p != NULL) {
                for(int iv=0; iv < p->vumeter_count;  ++iv) {
                    printf("%s\n", p->vumeters[iv].name);
                }
                p = p->next;
            }
            exit(EXIT_SUCCESS);
        } else if (0 == strcmp(argv[i], "dl")) {
            if (argc > i+1) {
                VUMeter_loadlib(argv[i+1]);
                i += 1;
            }
        } else if (0 == strcmp(argv[i], "json")) {
            if (argc > i+1) {
                app.context.json_file = argv[i+1];
                i += 1;
            }
        } else if (0 == strcmp(argv[i], "fs") || 0 == strcmp(argv[i], "fullscreen")) {
            app.context.fullscreen = true;
        } else if (0 == strcmp(argv[i], "help") || 0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "--h")) {
            puts(help_text);
            exit(EXIT_SUCCESS);
        } else if (0 == strcmp(argv[i], "peakhold")) {
            if (argc > i+1) {
                VUMeter_set_peak_hold(atoi(argv[i+1]));
                i += 1;
            }
        } else if (0 == strcmp(argv[i], "decayhold")) {
            if (argc > i+1) {
                VUMeter_set_decay_hold(atoi(argv[i+1]));
                i += 1;
            }
        } else if (0 == strcmp(argv[i], "texture_cache_size")) {
            if (argc > i+1) {
                tcache_set_limit(atoi(argv[i+1]));
                i += 1;
            }
        } else if (0 == strcmp(argv[i], "lms")) {
            if (argc > i+1) {
                app.context.lms = strdup(argv[i+1]);
                i += 1;
            } 
        } else {
            error_printf("Unknown command line option %d) %s\n", i, argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    if (VUMeter_get_props_list() == NULL) {
        error_printf("No VU Meters found\n");
        puts(help_text);
        exit(EXIT_FAILURE);
    }

    if (app.context.screen_width == 0) {
        app.context.screen_width = 800;
        app.context.screen_height = 480;
    }
    SDL_Thread* render_thread = SDL_CreateThread((SDL_ThreadFunction)sdl_render_loop, "render", &view);
    SDL_Thread* input_thread = SDL_CreateThread((SDL_ThreadFunction)sdl_input_loop, "input", &view);
    SDL_WaitThread(input_thread, NULL);
    SDL_WaitThread(render_thread, NULL);
    app_cleanup(&app.context, EXIT_SUCCESS);

    return 0;
}


