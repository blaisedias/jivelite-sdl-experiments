/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>
#include "vumeter_util.h"
#include "visualizer.h"
#include "util.h"
#include "timer.h"

// @60 FPS 30 => 1/2 a second
static int peak_hold_counter_init_value = 30;
// fine tune decay behaviour - default is 0 so decay immediately
// @60 FPS 4 appears to be a reasonable value.
static int decay_hold_counter_init_value = 0;

static int perf_level;
void VUMeter_set_perf_level(int l) {
    perf_level = l;
}

void VUMeter_set_peak_hold(int peak_hold) {
    peak_hold_counter_init_value = peak_hold;
}

void VUMeter_set_decay_hold(int decay_hold) {
    decay_hold_counter_init_value = decay_hold;
}


static void center_vu_element(SDL_Rect* outer, SDL_Rect* inner, SDL_Rect* dst, float orientation) {
    int dx = (outer->w - inner->w)/2;
    int dy = (outer->h - inner->h)/2;
    if (orientation == 90.0) {
        dx = 0 - dx;
    }
    if (orientation == 180.0) {
        dx = 0 - dx;
        dy = 0 - dy;
    }
    if (orientation == 270.0) {
        dy = 0 - dy;
    }
    dst->x += dx;
    dst->y += dy;
}


void VUMeter_orientate(vumeter_properties *vu, float rotation, SDL_Rect* enclosure) {
    int indx;
    vu->rotation = rotation;
    if (rotation == 0.0) {
        vumeter_element *ve = vu->placements.elements;
        SDL_Rect rbs = {
            .x=(enclosure->w-vu->w)/2, .y=(enclosure->h-vu->h)/2,
            .w=vu->w, .h=vu->h
        };
        debug_printf(" VUMeter_orientate: enclosure: {%4d,%4d,%4d,%4d}\n", enclosure->x, enclosure->y, enclosure->w, enclosure->h);
        debug_printf("                  : rotated  : {%4d,%4d,%4d,%4d}\n", rbs.x, rbs.y, rbs.w, rbs.h);
        for(indx = 0; indx < vu->placements.count; ++indx, ++ve) {
            center_vu_element(enclosure, &rbs, &ve->rect, 0.0);
        }
    } else if (rotation == 180.0) {
        vumeter_element *ve = vu->placements.elements;
        SDL_Rect rbs = {
            .x=(enclosure->w-vu->w)/2, .y=(enclosure->h-vu->h)/2,
            .w=vu->w, .h=vu->h
        };
        debug_printf(" VUMeter_orientate: enclosure: {%4d,%4d,%4d,%4d}\n", enclosure->x, enclosure->y, enclosure->w, enclosure->h);
        debug_printf("                  : rotated  : {%4d,%4d,%4d,%4d}\n", rbs.x, rbs.y, rbs.w, rbs.h);
        for(indx = 0; indx < vu->placements.count; ++indx, ++ve) {
            SDL_Rect* r = &ve->rect;;
            r->x = enclosure->w - r->x - r->w;
            r->y = enclosure->h - r->y - r->h;
            center_vu_element(enclosure, &rbs, &ve->rect, 180.0);
        }
    } else if (rotation == 90.0) {
        vumeter_element *ve = vu->placements.elements;
        SDL_Rect rbs = {
            .x=(enclosure->w-vu->h)/2, .y=(enclosure->h-vu->w)/2,
            .w=vu->h, .h=vu->w
        };
        debug_printf(" VUMeter_orientate: enclosure: {%4d,%4d,%4d,%4d}\n", enclosure->x, enclosure->y, enclosure->w, enclosure->h);
        debug_printf("                  : rotated  : {%4d,%4d,%4d,%4d}\n", rbs.x, rbs.y, rbs.w, rbs.h);
        for(indx = 0; indx < vu->placements.count; ++indx, ++ve) {
            SDL_Rect* r = &ve->rect;
            int x =  r->x;
            int y =  r->y;
            r->x = enclosure->w - r->h - y + ((r->h - r->w)/2);
            r->y = x + ((r->w - r->h)/2);
            center_vu_element(enclosure, &rbs, &ve->rect, 180.0);
        }
    } else if (rotation == 270.0) {
        vumeter_element *ve = vu->placements.elements;
        SDL_Rect rbs = {
            .x=(enclosure->w-vu->h)/2, .y=(enclosure->h-vu->w)/2,
            .w=vu->h, .h=vu->w
        };
        debug_printf(" VUMeter_orientate: enclosure: {%4d,%4d,%4d,%4d}\n", enclosure->x, enclosure->y, enclosure->w, enclosure->h);
        debug_printf("                  : rotated  : {%4d,%4d,%4d,%4d}\n", rbs.x, rbs.y, rbs.w, rbs.h);
        for(indx = 0; indx < vu->placements.count; ++indx, ++ve) {
            SDL_Rect* r = &ve->rect;
            int x =  r->x;
            int y =  r->y;
            r->x = y + ((r->h - r->w)/2);
            r->y = enclosure->h - r->w -x + ((r->w - r->h)/2);
            center_vu_element(enclosure, &rbs, &ve->rect, 270.0);
        }
    }
}

static char load_buffer[4096];
SDL_bool VUMeter_load_media(SDL_Renderer *renderer, vumeter_properties *vu) {
    int indx;
    uint64_t ms = getMillis();
    SDL_bool ok = SDL_TRUE;
    load_printf("load media: %p\n"
            "resources: count=%d names=%p textures=%p\n",
            vu,
            vu->resources.count,
            vu->resources.names,
            vu->resources.textures
    );
    for(indx = 0; indx < vu->resources.count; ++indx) {
        if (0 == vu->resources.textures[indx]) {
            if ( NULL != vu->resources.names[indx]) {
                int n = snprintf(load_buffer, sizeof(load_buffer), "%s/%s",
                        vu->resource_path, vu->resources.names[indx]);
                if (0 > n || n >= sizeof(load_buffer)) {
                    error_printf("snprintf %ld %s/%s/n",
                            sizeof(load_buffer),
                            vu->resource_path, vu->resources.names[indx]);
                    exit(EXIT_FAILURE);
                }
                bool loaded = false;
                vu->resources.textures[indx] = tcache_load_media(load_buffer, renderer, &loaded);
                ok = ok && loaded;
            } else {
                // if no texture is associated with a slot point to the empty entry, this 
                //  - prevents error messages associated with retrieving texture for unintialised texture id
                //  - allows use of the texture id slot without additional checks
                vu->resources.textures[indx] = tcache_get_empty_tid();
            }
        } else {
            ok = ok && tcache_load_from_file(vu->resources.textures[indx], renderer);
        }
    }
    ms = getMillis() - ms;
    perf_printf("load media %s time:%lu milliseconds ok=%s\n",
                vu->name,
                ms,
                ok?"true":"false");
    return ok;
}

void VUMeter_unload_media(vumeter_properties *vu) {
    if (vu) {
        load_printf("unload media: %p\n"
                "resources: count=%d names=%p textures=%p\n",
                vu,
                vu->resources.count,
                vu->resources.names,
                vu->resources.textures
        );
        for(int indx = 0; indx < vu->resources.count; ++indx) {
            tcache_quick_delete_texture(vu->resources.textures[indx]);
            vu->resources.textures[indx] = 0;
        }
    }
}

static void release_properties(vumeter_properties *vu) {
    if (vu) {
        VUMeter_unload_media(vu);
        free(vu->placements.elements);
        free(vu->resources.textures);
        // created by strdup assigned to const char*
        free((void *)(vu->resource_path));
        free(vu);
    }
}

char* VUMeter_resource_path(const char *root, vumeter_properties* vu) {
    if (vu->name != NULL) {
        int n = snprintf(load_buffer, sizeof(load_buffer), "%s/%s", root, vu->name);
        if (0 > n || n >= sizeof(load_buffer)) {
            error_printf("snprintf ");
            exit(EXIT_FAILURE);
        }
    } else {
        int n = snprintf(load_buffer, sizeof(load_buffer), "%s", root);
        if (0 > n || n >= sizeof(load_buffer)) {
            error_printf("snprintf ");
            exit(EXIT_FAILURE);
        }
    }
    return strdup(load_buffer);
}

vumeter_properties* VUMeter_scale(vumeter_properties* vu, int w_in, int h_in, float rotation, const char* resource_path) {
    int w = w_in;
    int h = h_in;
    if (rotation == 0.0 || rotation == 180.0) {
    } else if (rotation == 90.0 || rotation == 270.0) {
//        w = h_in;
//        h = w_in;
    } else {
        error_printf("Unsupported rotation %f\n", rotation);
        exit(EXIT_FAILURE);
    }

#define VU_SCALEV(v) (int)(scalef * (v))
    float scalef = MIN((float)w/(float)vu->w, (float)h/(float)vu->h);
    int scaled_w = VU_SCALEV(vu->w);
    int scaled_h = VU_SCALEV(vu->h);
    int dx = 0;
    int dy = 0;
//    dx = (w-scaled_w)/2;
//    dy = (h-scaled_h)/2;
    scale_printf(
            "scale request:%dx%d"
            " actual:%dx%d -> %dx%d"
            " deltaxy:%d,%d"
            " scale_factor:%f"
            "\n",
            w, h,
            vu->w, vu->h,
            scaled_w, scaled_h,
            dx,dy,
            scalef
            );


    vumeter_properties* resized_vu = calloc(1, sizeof(*resized_vu));
    if (NULL == resized_vu) {
        error_printf("OOM vumeter_properties\n");
        return NULL;
    }

    {
        // initialise resized vu meter struct, this method preserves the const attributes
        // whilst allowing insitialisation of the dynamically allocated vu meter struct
        vumeter_properties scaled_props = {
            .name = vu->name,
            .volume_levels = vu->volume_levels,
            .w = scaled_w,
            .h = scaled_h,
            .resources = {
                .count = vu->resources.count,
                .names = vu->resources.names,
                .textures = NULL,
            },
            .vumeters = vu->vumeters,
            .placements = {
                .count = vu->placements.count,
                .elements = NULL,
            },
//            .fn_release = &release_properties,
            .vumeter_count = vu->vumeter_count,
        };

//        scaled_props.resource_path = VUMeter_resource_path(resource_path, &scaled_props);
        scaled_props.resource_path = strdup(vu->resource_path);
        memcpy(resized_vu, &scaled_props, sizeof(*resized_vu));
    }

    resized_vu->resources.textures = calloc(vu->resources.count, sizeof(resized_vu->resources.textures[0]));
    if (NULL == resized_vu->resources.textures ) {
        error_printf("OOM textures\n");
        release_properties(resized_vu);
        return  NULL;
    }

    resized_vu->placements.elements = calloc(vu->placements.count, sizeof(resized_vu->placements.elements[0]));
    if (NULL == resized_vu->placements.elements ) {
        error_printf("OOM placements\n");
        release_properties(resized_vu);
        return  NULL;
    }

    for(int indx = 0; indx < vu->placements.count; ++indx) {
        vumeter_element *src_elem = vu->placements.elements + indx;
        vumeter_element *dst_elem = resized_vu->placements.elements + indx;
        dst_elem->rect.x = dx + VU_SCALEV(src_elem->rect.x);
        dst_elem->rect.y = dy + VU_SCALEV(src_elem->rect.y);
        dst_elem->rect.w = VU_SCALEV(src_elem->rect.w);
        dst_elem->rect.h = VU_SCALEV(src_elem->rect.h);
        dst_elem->texture_index = src_elem->texture_index;
    }
#undef VU_SCALE
#if     0
    static char command_buffer[10240];
    sprintf(command_buffer, "mkdir -p %s", resized_vu->resource_path);
    if (system(command_buffer)) {
        error_printf("failed: %s", command_buffer);
        release_properties(resized_vu);
        return NULL;
    }
    for(int indx = 0; indx < vu->placements.count; ++indx) {
        int resource_id =  vu->placements.elements[indx].texture_index;
        if (vu->resources.names[resource_id] != NULL) {
            {
                sprintf(command_buffer, "%s/%s", resized_vu->resource_path, resized_vu->resources.names[resource_id]);
                FILE* fd = fopen(command_buffer, "r");
                if (fd != NULL) {
                    fclose(fd);
                    continue;
                }
            }

            int n = snprintf(command_buffer, sizeof(command_buffer),
                    "magick convert -resize %dx%d +repage %s/%s %s/%s >& /dev/null",
                    resized_vu->placements.elements[indx].rect.w,
                    resized_vu->placements.elements[indx].rect.h,
                    vu->resource_path, vu->resources.names[resource_id],
                    resized_vu->resource_path, resized_vu->resources.names[resource_id]
            );
            if (0 > n || n >= sizeof(command_buffer)) {
                error_printf("snprintf "
                    "magick convert -resize %dx%d +repage %s/%s %s/%s\n >& /dev/null",
                    resized_vu->placements.elements[indx].rect.w,
                    resized_vu->placements.elements[indx].rect.h,
                    vu->resource_path, vu->resources.names[resource_id],
                    resized_vu->resource_path, resized_vu->resources.names[resource_id]
                );
            }
            puts(command_buffer);
            if (system(command_buffer)) {
                error_printf("failed: %s", command_buffer);
                release_properties(resized_vu);
                return NULL;
            }
        }
    }
#endif
    return resized_vu;
}

static SDL_RendererFlip flip = SDL_FLIP_NONE;
static uint64_t frame_count;
static uint32_t sample_frame_count;
static uint64_t acc_render_time;
static uint64_t max_render_time;
static uint64_t ms_1;
static uint64_t ms_2;
// to check and reset performance counters when vumeter is changed.
static const vumeter* prev_vumeter;


void VUMeter_draw(SDL_Renderer *renderer, vumeter_properties *vu, const vumeter* vumeter, int* vols, SDL_Rect* enclosure) {
//    SDL_RenderClear(renderer);

    if (perf_printf != dummy_printf) {
       if(prev_vumeter != vumeter) {
            max_render_time = 0;
            sample_frame_count = 0;
            frame_count = 0;
            acc_render_time = 0;
            ms_1 = getMicros();
        }
        prev_vumeter = vumeter;
    }

    uint64_t ms0 = getMicros();

    int i;

    runtime_volume *runtimes[2] = {
        &vumeter->channels[0]->runtime,
        &vumeter->channels[1]->runtime,
    };

    runtimes[0]->vol = vols[0];
    runtimes[1]->vol = vols[1];

    SDL_Rect render_rect;

    for (i=0; i < 2; ++i) {
        if (runtimes[i]->vol > runtimes[i]->peak_hold_vol) {
//            runtimes[i].peak_hold_counter = peak_hold_counter_start;
            runtimes[i]->peak_hold_counter = peak_hold_counter_init_value;
            runtimes[i]->peak_hold_vol = runtimes[i]->vol;
        }
        if (--runtimes[i]->peak_hold_counter < 0) {
            runtimes[i]->peak_hold_vol = 0;
            runtimes[i]->peak_hold_counter = 0;
        }
        if (runtimes[i]->vol > runtimes[i]->decay_vol) {
            runtimes[i]->decay_vol = runtimes[i]->vol;
            runtimes[i]->decay_hold_counter = decay_hold_counter_init_value;
        } else {
            if (--runtimes[i]->decay_hold_counter < 0) {
                runtimes[i]->decay_vol -= runtimes[i]->decay_unit;
                runtimes[i]->decay_hold_counter = 0;
            }
        }
    }

    if (vumeter->background) {
        const int *bg = vumeter->background->bg;
        while(bg != NULL && 0 != *bg) {
            vumeter_element *p = &vu->placements.elements[*bg];
            rebaseRect(enclosure, &p->rect, &render_rect);
            SDL_RenderCopyEx(renderer,
                    tcache_quick_get_texture(vu->resources.textures[p->texture_index]),
                    NULL, &render_rect, vu->rotation, NULL, flip);
            ++bg;
        }
    }

#define _RENDER_VOLUME_LEVEL_(value) \
        rebaseRect(enclosure, &vu->placements.elements[comp->placements[value]].rect, &render_rect); \
        SDL_RenderCopyEx(renderer,\
        tcache_quick_get_texture(vu->resources.textures[vu->placements.elements[comp->placements[value]].texture_index]),\
        NULL,\
        &render_rect,\
        vu->rotation, NULL, flip)

    for(i=0; i<2; ++i) {
        vol_printf("%2d) ", i);
        channel* channel = vumeter->channels[i];
        const component* comp = channel->components;
        runtime_volume *runtime = runtimes[i];
        for(int ic=0; ic < channel->component_count; ++ic, ++comp) {
            switch(comp->render) {
                case SINGLE:
                    {
                        switch(comp->peak) {
                            case PEAK_NONE:
                                vol_printf("SPN:%02d ", runtime->vol);
                                _RENDER_VOLUME_LEVEL_(runtime->vol);
                                break;
                            case DECAY:
                                vol_printf("SD:%02d ", (int)runtime->decay_vol);
                                _RENDER_VOLUME_LEVEL_((int)runtime->decay_vol);
                                break;
                            case HOLD_DECAY:
                                vol_printf("SHD:%02d ", (int)runtime->decay_vol);
                                _RENDER_VOLUME_LEVEL_((int)runtime->decay_vol);
                            case HOLD:
                                vol_printf("SH:%02d ", runtime->peak_hold_vol);
                                _RENDER_VOLUME_LEVEL_(runtime->peak_hold_vol);
                                break;
                        }
                    }break;
                case AGGREGATE:
                    {
                        int vol;
                        int peak_vol;
                        switch(comp->peak) {
                            case PEAK_NONE:
                                vol = runtime->vol;
                                peak_vol = 0;
                                vol_printf("A v:%02d ", vol);
                                break;
                            case HOLD_DECAY:
                                vol = (int)runtime->decay_vol;
                                peak_vol = runtime->peak_hold_vol;
                                vol_printf("A v:%02d p:%02d ", vol, peak_vol);
                                break;
                            case DECAY:
                                vol = (int)runtime->decay_vol;
                                peak_vol = 0;
                                vol_printf("A v:%02d ", vol);
                                break;
                            case HOLD:
                                vol = runtime->vol;
                                peak_vol = runtime->peak_hold_vol;
                                vol_printf("A v:%02d p:%02d ", vol, peak_vol);
                                break;
                        }

                        for(int lvl=0; lvl <= vol; ++lvl) {
                            _RENDER_VOLUME_LEVEL_(lvl);
                        }
                        if (peak_vol > vol) {
                           _RENDER_VOLUME_LEVEL_(peak_vol);
                        }
                    }break;
                case AGGREGATEOFF:
                    {
                        int vol = 0;
                        switch(comp->peak) {
                            case PEAK_NONE:
                            case HOLD:
                                vol = runtime->vol;
                                break;
                            case DECAY:
                            case HOLD_DECAY:
                                vol = (int)runtime->decay_vol;
                                break;
                        }
                        vol_printf("a v:%02d ", vol);
                        for(int lvl=vol+1; lvl< vu->volume_levels; ++lvl) {
                            _RENDER_VOLUME_LEVEL_(lvl);
                        }
                    }break;
            }
        }
    }
    vol_printf("\r");

#undef _RENDER_VOLUME_LEVEL_
    uint64_t delta_pf = getMicros() - ms0;
    acc_render_time += delta_pf;

    if (delta_pf > max_render_time) {
        max_render_time =  delta_pf;
//        printf("%ld\n", max_render_time);
    }
    ++sample_frame_count;
    ++frame_count;
    if (sample_frame_count >= 100) {
        ms_2 = getMicros();
        float fps = 1000000.0 * sample_frame_count/(ms_2-ms_1);
        switch(perf_level) {
            case 3:
            case 2:
            default:
                perf_printf("\rFPS:%05.2f frame:millis: avg:%5.2f, max:%05.2f sample: millis:%5.2f, frames:%d",
                        fps,
                        ((float)acc_render_time/sample_frame_count)/1000,
                        (float)max_render_time/1000,
                        (float)(ms_2 - ms_1)/1000, sample_frame_count
                    );
        }
        sample_frame_count = 0;
        acc_render_time = 0;
        ms_1 = ms_2;
//        max_render_time = 0;
    }
}

void VUMeter_diag() {
    printf("visualizer rate= %u\n", vis_get_rate());
}


void VUMeter_dump_props(const vumeter_properties* props) {
    printf("%p\nresource_path=%s name=%s volume_levels=%d w=%d h=%d vumeters=%p\n",
            props,
            props->resource_path,
            props->name,
            props->volume_levels, props->w, props->h,
            props->vumeters
           );
    printf("resources: count=%d names=%p textures=%p\n",
            props->resources.count,
            props->resources.names,
            props->resources.textures
           );
    printf("placements: count=%d elements=%p\n",
            props->placements.count,
            props->placements.elements
           );
    for(int indx=0; indx < props->resources.count; ++indx) {
        printf("%d) texture=%d %s\n",
                indx,
                 props->resources.textures[indx],
                 props->resources.names[indx]
               );
    }
    const vumeter* vumeter = props->vumeters;
    const int *bg = vumeter->background->bg;
    while(bg != NULL && 0 != *bg) {
        vumeter_element *p = &props->placements.elements[*bg];
        printf("bg, texture_index=%02d texture=%d, rect=(%d, %d, %d, %d)\n",
                p->texture_index,
                props->resources.textures[p->texture_index],
                p->rect.x, p->rect.y, p->rect.w, p->rect.h
               );
        ++bg;
    }
    for(int i=0; i<2; ++i) {
        channel* channel = vumeter->channels[i];
        const component* comp = channel->components;
        printf("channel %d, components count %d\n", i, channel->component_count);
        for(int ic=0; ic < channel->component_count; ++ic, ++comp) {
            printf("channel %d), component %d) %d, %d %p\n", i, ic, comp->render, comp->peak, comp);
            for(int value =0; value < props->volume_levels; ++value) {
                int pi = comp->placements[value];
                int ti = props->placements.elements[pi].texture_index;
                SDL_Rect *r = &props->placements.elements[pi].rect;
                printf("\tlevel=%d placement_index=%d texture_index=%d texture=%d rect=(%d,%d,%d,%d) %s\n",
                        value,
//                        props->placements.elements[value].texture_index,
//                        props->resources.textures[props->placements.elements[value].texture_index],
                        pi,
                        ti,
                        props->resources.textures[ti],
                        r->x, r->y, r->w, r->h,
                        props->resources.names[ti]
                       );
            }
        }
    }
}

