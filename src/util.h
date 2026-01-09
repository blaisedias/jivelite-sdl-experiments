#include <SDL2/SDL.h>
#ifndef __util_h_
#define __util_h_
#include "types.h"

typedef struct {
    float           orientation;
    SDL_Rect        window_rect;
    SDL_Renderer*   renderer;
    Uint32          pixelFormat;
    SDL_Window*     window;
    bool            fullscreen;

    int             screen_width;
    int             screen_height;
    int             max_iters;
    int             cycle_iters;
    int             delay;
    bool            vsync;
} app_context;

extern void copyRect(const SDL_Rect *src, SDL_Rect *dst);

extern const void (*translate_xy)(int* x, int* y);
extern const void (*translate_point)(SDL_Point* pt);
extern const void (*translate_image_rect)(SDL_Rect* rect);
extern const void (*translate_draw_rect)(SDL_Rect* rect);
extern void setup_orientation(float orientation, int w, int h, SDL_Rect* screen);

void rebaseRect(SDL_Rect* origin, SDL_Rect* src, SDL_Rect* dst);
#endif
