#include <SDL2/SDL.h>
#ifndef __jl_application_h_
#define __jl_application_h_
#include "types.h"
#include "lyrion_player.h"

typedef struct {
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
} app_context;

#endif // __jl_application_h_
