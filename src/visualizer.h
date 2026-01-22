#ifndef __jl_visualiser_h
#define __jl_visualiser_h
#include "types.h"

extern void vis_check(void);
extern void vis_lock(void);
extern void vis_unlock(void);

extern bool vis_get_playing(void);
extern u32_t vis_get_rate(void);

extern s16_t *vis_get_buffer(void);
extern u32_t vis_get_buffer_len(void);
extern u32_t vis_get_buffer_idx(void);
extern int visualizer_vumeter(int* levels);
#endif //__jl_visualiser_h

