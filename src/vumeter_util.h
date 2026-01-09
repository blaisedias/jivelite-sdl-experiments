/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#ifndef _VUMETER_UTIL_H_
#define _VUMETER_UTIL_H_
#include "logging.h"
#include "vumeterdef.h"
#include "types.h"

char* VUMeter_resource_path(const char *root, vumeter_properties* vu);

vumeter_properties* VUMeter_scale(vumeter_properties* vu, int w, int h, float rotation, const char* path);
void VUMeter_orientate(vumeter_properties *vu, float rotation, SDL_Rect* rect);

SDL_bool VUMeter_load_media(SDL_Renderer *renderer, vumeter_properties *vu);
void VUMeter_unload_media(vumeter_properties *vu);

void VUMeter_draw(SDL_Renderer *renderer, vumeter_properties *vu, const vumeter* vumeter, int* vols, SDL_Rect* enclosure);

void VUMeter_dump_props(const vumeter_properties* vu);

void VUMeter_diag();

void VUMeter_set_perf_level(int);

bool VUMeter_loadlib(const char* path);
const vumeter_properties* VUMeter_get_props_list();

void VUMeter_set_peak_hold(int peak_hold);
void VUMeter_set_decay_hold(int decay_hold);

#endif

