#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>
//#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>
#include <time.h>
#include "vumeter_util.h"
#include "application.h"
#include "widgets.h"
#include "visualizer.h"

static vumeter_properties* vu_props_list;

//static struct {
//    vumeter_properties *props;
//    const vumeter* meter;
//} meters[100];
//static int num_meters=0;

struct vumeter_widget {
    vumeter_properties* props;
    struct {
        vumeter_properties *props;
        const vumeter* meter;
    } meters[100];
    int num_meters;
    int meter_indx;
    bool locked;
};

const vumeter_properties* VUMeter_get_props_list() {
    return vu_props_list;
}

bool VUMeter_loadlib(const char* path) {
    void* handle = dlopen(path, RTLD_NOW);
    dlerror();
    if (handle == NULL) {
        error_printf("failed to load %s\n", path);
//        exit(EXIT_FAILURE);
        return false;
    }
    vumeter_properties* vp = dlsym(handle, "VuProperties");
    if (vp == NULL) {
        dlerror();
        dlclose(handle);
        error_printf(" failed to find symbol VuProperties in %s\n", path);
//        exit(EXIT_FAILURE);
        return false;
    }
    vp->handle = handle;
    if (vu_props_list == NULL) {
        vu_props_list = vp;
    } else {
        vp->next = vu_props_list;
        vu_props_list = vp;
    }
    return true;
}

void vumeter_widget_load_media(widget *wdgt, const char* resource_path) {
    vumeter_properties* base_props = vu_props_list;
    char buffer[1024];
    sprintf(buffer, "./images/runtime/%dx%d", wdgt->rect.w, wdgt->rect.h);
    vumeter_widget* vw = wdgt->sub.vu;
    vw->num_meters = 0;
    debug_printf("VU Meter widget:\n");
    debug_printf("          rect = {%4d,%4d,%4d,%4d}\n", wdgt->rect.x, wdgt->rect.y, wdgt->rect.w, wdgt->rect.h);
    while(NULL != base_props) {
        base_props->resource_path = VUMeter_resource_path(resource_path, base_props);
        vumeter_properties* props = VUMeter_scale(base_props,
                wdgt->rect.w, wdgt->rect.h,
                wdgt->view->app->orientation, buffer);
#ifdef  VUMETERS_CHECK_ON_INIT
        if (!VUMeter_load_media(wdgt->view->app->renderer, props)) {
            error_printf("failed to load media for %s\n", props->name);
            continue;
        }
#endif
        SDL_Rect draw_rect;
        copyRect(&wdgt->rect, &draw_rect);
        translate_draw_rect(&draw_rect);
        VUMeter_orientate(props, wdgt->view->app->orientation, &draw_rect);
#ifdef  VUMETERS_CHECK_ON_INIT
        VUMeter_unload_media(props);
#endif
        const vumeter* meter = props->vumeters;
        float decay_unit = (float)props->volume_levels/60;
        for(int ix = 0; ix < props->vumeter_count; ++ix, ++(vw->num_meters), ++meter) {
            for(int ch=0; ch < 2; ++ch) {
                if (meter->channels[ch]) {
                    meter->channels[ch]->runtime.decay_unit = decay_unit;
                }
            }
            vw->meters[vw->num_meters].props = props;
            vw->meters[vw->num_meters].meter = meter;
            debug_printf("    %d) meter:%s decay_unit:%f\n", vw->num_meters, vw->meters[vw->num_meters].meter->name, decay_unit);
        }
        base_props = base_props->next;
    }
    if (!VUMeter_load_media(wdgt->view->app->renderer, vw->meters[vw->meter_indx].props)) {
        error_printf("failed to load media for %s\n",  vw->meters[vw->meter_indx].props->name);
    }
}

extern void _show_draw_rect(widget* wdgt);
extern void _show_input_rect(widget* wdgt);

static void vumeter_render(widget* wdgt) {
    if (wdgt->highlight) {
        if (show_rects) { _show_draw_rect(wdgt); }
        if (show_input_rects) { _show_input_rect(wdgt); }
    }
    SDL_Rect draw_rect;
    copyRect(&wdgt->rect, &draw_rect);
    translate_draw_rect(&draw_rect);
    vumeter_widget* vw = wdgt->sub.vu;
    int vols[2];
    visualizer_vumeter(vols);
    VUMeter_draw(wdgt->view->app->renderer,vw->meters[vw->meter_indx].props,vw->meters[vw->meter_indx].meter, vols, &draw_rect);
}

widget *widget_create_vumeter(const view_context* view) {
    widget* wdgt = calloc(sizeof(*wdgt), 1);
    if (wdgt) {
        wdgt->view = view;
        wdgt->action = ACTION_NONE;
        wdgt->render = vumeter_render;
        wdgt->sub.vu = calloc(1, sizeof(vumeter_widget));
        if (wdgt->sub.vu == NULL) {
            widget_destroy(wdgt);
            wdgt = NULL;
        } else {
            *((widget_type*)&wdgt->type) = WIDGET_VUMETER;
            if (view->list) {
                wdgt->next = &view->list->tail;
                wdgt->prev = view->list->tail.prev;
                wdgt->prev->next = wdgt->next->prev = wdgt;
            }
        }
    }
    return wdgt;
}

static void free_vumeter_widget_prop(vumeter_widget* vw, vumeter_properties* props) {
    if (props) {
        // let texture cache handle release of textures on demand
        //VUMeter_unload_media(props);
        for (int iix=0; iix<vw->num_meters; ++iix) {
            if (props == vw->meters[iix].props) {
                vw->meters[iix].props = NULL;
            }
        }
        free(props);
    }
}

widget *vumeter_widget_destroy(widget *wdgt) {
    if (wdgt == NULL) {
        return wdgt;
    }
    vumeter_widget* vw = wdgt->sub.vu;
    if (vw) {
        for (int ix=0; ix<vw->num_meters; ++ix) {
            free_vumeter_widget_prop(vw, vw->meters[ix].props);
        }
        free(vw);
        wdgt->sub.vu = (void *)NULL;
    }
    return wdgt;
}

static bool vumeter_select(widget *wdgt, int indx) {
    vumeter_widget* vw = wdgt->sub.vu;
    if (indx < 0 || indx >= vw->num_meters) {
        return false;
    }
    perf_printf("\n");
    if (indx != vw->meter_indx)
    {
        // let texture cache handle release of textures on demand
        //VUMeter_unload_media(vw->meters[vw->meter_indx].props);
        vumeter_properties* props = vw->meters[indx].props;
        if (!VUMeter_load_media(wdgt->view->app->renderer, props)) {
            exit(EXIT_FAILURE);
        }
    }
    vw->meter_indx = indx;
    debug_printf("vumeter: %s\n", vw->meters[vw->meter_indx].meter->name);
    return true;
}

widget *widget_vumeter_select_next(widget *wdgt) {
    if (wdgt == NULL) {
        return wdgt;
    }
    vumeter_widget* vw = wdgt->sub.vu;
    if (vw->locked) {
        return wdgt;
    }
    vumeter_select(wdgt, (vw->meter_indx + 1) % vw->num_meters);
    return wdgt;
}

widget *widget_vumeter_select_prev(widget *wdgt) {
    if (wdgt == NULL) {
        return wdgt;
    }
    vumeter_widget* vw = wdgt->sub.vu;
    if (vw->locked) {
        return wdgt;
    }
    vumeter_select(wdgt, vw->meter_indx == 0 ? vw->num_meters-1 : vw->meter_indx-1);
    return wdgt;
}

widget *widget_vumeter_select_by_name(widget *wdgt, const char* name) {
    if (wdgt == NULL) {
        return wdgt;
    }
    vumeter_widget* vw = wdgt->sub.vu;
    if (vw->locked || name == NULL) {
        return wdgt;
    }
    for (int indx=0; indx < vw->num_meters; ++indx) {
        if (0 == strcmp(name, vw->meters[indx].meter->name)) {
            vumeter_select(wdgt, indx);
        }
    }
    return wdgt;
}

widget *widget_vumeter_select_lock(widget *wdgt, bool lock) {
    vumeter_widget* vw = wdgt->sub.vu;
    vw->locked = lock;
    return wdgt;
}


