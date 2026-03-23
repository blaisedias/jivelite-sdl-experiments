/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#ifndef __jl_vumeterdef_h_
#define __jl_vumeterdef_h_
#include <SDL2/SDL.h>
#include "texture_cache.h"

typedef struct {
//    const char* image;
    int         texture_index;
    SDL_Rect    rect;
} vumeter_element;

typedef enum {
    SINGLE,
    AGGREGATE,
    AGGREGATEOFF,
} render_typ;

typedef enum {
    PEAK_NONE,
    HOLD,
    DECAY,
    HOLD_DECAY,
}peak_typ;

typedef struct {
    const char* imagefile;
    int w;
    int h;
}resource;

typedef struct {
    const int* bg;
}background;

typedef struct {
    const render_typ  render;
    const peak_typ    peak;
    const int placements[50];
}component;

typedef struct {
    int vol;
    int peak_hold_vol;
    int peak_hold_counter;
    int decay_hold_counter;
    float decay_vol;
    float decay_unit;
}runtime_volume;

typedef struct {
    const int component_count;
    const component* components;
    runtime_volume runtime;
}channel;

typedef struct {
    const char* name;
    const background*   background;
    channel*      channels[2];
}vumeter;

typedef struct vu_props {
    struct vu_props* next;
    const char* resource_path;
    const char* name;
    const int volume_levels;
    const int w;
    const int h;
    const int vumeter_count;
    const vumeter* vumeters;
    struct {
        const int count;
        const char** names;
        texture_id_t* textures;
    }resources;
    struct {
        const int count;
        vumeter_element* elements;
    }placements;
    float rotation;
    void * handle;
}vumeter_properties;

#endif  // __jl_vumeterdef_h_
