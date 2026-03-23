#include <SDL2/SDL.h>
#ifndef __jl_application_h_
#define __jl_application_h_
#include "types.h"
#include "lyrion_player.h"

typedef struct {
    unsigned  reported_fps;
    player_mode_t   player_mode;
    int64_t         player_mode_start_timestamp;
} app_workspace_t;

typedef struct app_context {
    float           orientation;
    SDL_Rect        window_rect;
    SDL_Renderer*   renderer;
    const SDL_threadID   renderer_tid;
    Uint32          pixelFormat;
    unsigned        bytes_per_pixel;
    SDL_Window*     window;
    bool            fullscreen;

    int             screen_width;
    int             screen_height;
    int             max_secs;
    int             cycle_secs;
    int             vsync;

    int             refresh_rate;
    int             frame_time_millis;
    int             frame_time_micros;

    bool            profile_fps_deviation;
    const           char* lms;
    player_ptr      player;

    const char*     window_title;
    const char*     json_file;
    bool            dump_vu;
    const char*     first_vu_meter;

    bool            ready;
    const char*     default_font_path;

    int             max_texture_width;
    int             max_texture_height;

    app_workspace_t workspace; 
} app_context;

void app_cleanup(app_context* app, int exit_status);
void print_app_runtime_info(app_context* app_ctx);

#endif // __jl_application_h_
