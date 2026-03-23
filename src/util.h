#include <SDL2/SDL.h>
#ifndef __jl_util_h_
#define __jl_util_h_


extern const void (*translate_xy)(int* x, int* y);
extern const void (*translate_point)(SDL_Point* pt);
extern const void (*translate_image_rect)(SDL_Rect* rect);
extern const void (*translate_draw_rect)(SDL_Rect* rect);
extern void setup_orientation(float orientation, int w, int h, SDL_Rect* screen);

extern void copyRect(const SDL_Rect *src, SDL_Rect *dst);
void rebaseRect(SDL_Rect* origin, SDL_Rect* src, SDL_Rect* dst);
#endif // __jl_util_h_
