#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>
#include "widgets.h"
#include "actions.h"
#include "util.h"
#include "logging.h"

extern widget *vumeter_widget_destroy(widget *wdgt);
extern void vumeter_widget_load_media(widget *wdgt, const char* resource_path);

bool show_rects = false;
bool show_input_rects = false;

static SDL_RendererFlip flip = SDL_FLIP_NONE;

static char* widget_type_strings[] = {
    "None",
    "Image",
    "Button",
    "MultistateButton",
    "VUMeter",
    "Slider",
    "none"
};

const char* widget_type_name(widget_type typ) {
    if (typ >= WIDGET_NONE && typ <= WIDGET_END) {
        return widget_type_strings[typ];
    }
    return "";
}

void render_none(widget* btn) {
}

void _show_draw_rect(widget* wdgt) {
    if (wdgt) {
        SDL_Rect draw_rect;
        copyRect(&wdgt->rect, &draw_rect);
        translate_draw_rect(&draw_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 128, 128, 64, 32);
        SDL_RenderDrawRect(wdgt->view->app->renderer, &draw_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 0, 0, 0, 0);
    }
}

void _show_input_rect(widget* wdgt) {
    if (wdgt) {
        SDL_Rect input_rect;
        copyRect(&wdgt->input_rect, &input_rect);
        translate_draw_rect(&input_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 128, 0, 0, 32);
        SDL_RenderDrawRect(wdgt->view->app->renderer, &input_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 0, 0, 0, 0);
    }
}

static void button_widget_render(widget* wdgt) {
    if (wdgt->pressed && !wdgt->hotspot) {
        SDL_Rect draw_rect;
        copyRect(&wdgt->rect, &draw_rect);
        translate_draw_rect(&draw_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 128, 128, 128, 16);
        SDL_RenderFillRect(wdgt->view->app->renderer, &draw_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 0, 0, 0, 0);
    }
    if (wdgt->highlight && wdgt->hotspot == false && show_rects) {
        _show_draw_rect(wdgt);
    }
    if (wdgt->highlight && show_input_rects) {
        _show_input_rect(wdgt);
    }
    if (wdgt->hotspot == false || wdgt->highlight)  {
        SDL_Rect image_rect;
        copyRect(&wdgt->rect, &image_rect);
        translate_image_rect(&image_rect);
        SDL_RenderCopyEx(wdgt->view->app->renderer, wdgt->sub.button.texture, NULL,
            &image_rect, wdgt->view->app->orientation, NULL, flip);
    }
}

static void setup_image_fit_src_rect(widget *wdgt) {
    if (wdgt->type == WIDGET_IMAGE) {
        switch(wdgt->sub.image.scale_op)
        {
            case IMAGE_STRETCH_FILL:
                break;
            case IMAGE_CENTRED_FILL: {
                float scale_f = MAX((float)(wdgt->rect.w)/wdgt->sub.image.w, (float)(wdgt->rect.h)/wdgt->sub.image.h);
                wdgt->sub.image.src_rect.w = wdgt->rect.w/scale_f;
                wdgt->sub.image.src_rect.h = wdgt->rect.h/scale_f;
                wdgt->sub.image.src_rect.x = (wdgt->sub.image.w -  wdgt->sub.image.src_rect.w)/2;
                wdgt->sub.image.src_rect.y = (wdgt->sub.image.h -  wdgt->sub.image.src_rect.h)/2;
                debug_printf("image widget: centered_fill src: {w=%d, h=%d} %f, scalef=%f {%d,%d,%d,%d}\n", 
                        wdgt->sub.image.w, wdgt->sub.image.h, 
                        (float)wdgt->sub.image.w/wdgt->sub.image.h,
                        scale_f,
                        wdgt->sub.image.src_rect.x, wdgt->sub.image.src_rect.y, wdgt->sub.image.src_rect.w, wdgt->sub.image.src_rect.h
                        );
                }break;
            case IMAGE_FIT: {
                float scale_f = MIN((float)(wdgt->rect.w)/wdgt->sub.image.w, (float)(wdgt->rect.h)/wdgt->sub.image.h);

                wdgt->sub.image.dst_rect.w = wdgt->sub.image.w*scale_f;
                wdgt->sub.image.dst_rect.h = wdgt->sub.image.h*scale_f;
                wdgt->sub.image.dst_rect.x = (wdgt->rect.w -  wdgt->sub.image.dst_rect.w)/2;
                wdgt->sub.image.dst_rect.y = (wdgt->rect.h -  wdgt->sub.image.dst_rect.h)/2;
                debug_printf("image widget: dst: fit: {w=%d, h=%d} %f, scalef=%f {%d,%d,%d,%d}\n", 
                        wdgt->sub.image.w, wdgt->sub.image.h, 
                        (float)wdgt->sub.image.w/wdgt->sub.image.h,
                        scale_f,
                        wdgt->sub.image.dst_rect.x, wdgt->sub.image.dst_rect.y, wdgt->sub.image.dst_rect.w, wdgt->sub.image.dst_rect.h
                        );
                if (wdgt->view->app->orientation == 90.0 || wdgt->view->app->orientation == 270.0) {
                    translate_image_rect(&wdgt->sub.image.dst_rect);
                }
              }break;
        }
    }
}

widget* widget_load_media(widget* wdgt, const char* resource_path) {
    if (wdgt && wdgt->view->app->renderer) {
        switch(wdgt->type) {
            case WIDGET_NONE:
            case WIDGET_END:
                break;
            case WIDGET_VUMETER:
                vumeter_widget_load_media(wdgt, resource_path);
                break;
            case WIDGET_IMAGE:
                {
                    wdgt->sub.image.texture = IMG_LoadTexture(wdgt->view->app->renderer, wdgt->image_path);
                    if (wdgt->sub.image.texture) {
                        if (0 == SDL_QueryTexture(wdgt->sub.image.texture, NULL, NULL, &wdgt->sub.image.w, &wdgt->sub.image.h)) {
                            setup_image_fit_src_rect(wdgt);
                        }
                    } else {
                        error_printf("widget_load_media: image failed to load %s\n", wdgt->image_path);
                    }
                }break;
            case WIDGET_BUTTON:
                wdgt->sub.button.texture = IMG_LoadTexture(wdgt->view->app->renderer, wdgt->image_path);
                if (wdgt->sub.button.texture == NULL) {
                    error_printf("widget_load_media: button failed to load %s\n", wdgt->image_path);
                }
                break;
            case WIDGET_MULTISTATE_BUTTON:
                for(int ims=0; ims < wdgt->sub.multistate_button.state_count; ++ims) {
                    _btn_resource* res = wdgt->sub.multistate_button.res + ims;
                    res->texture = IMG_LoadTexture(wdgt->view->app->renderer, res->resource_path);
                    if (res->texture == NULL) {
                        error_printf("widget_load_media: multistate button failed to load %s\n", res->resource_path);
                    }
                }
                break;
            case WIDGET_SLIDER:
                for(int ix=0; ix<SLIDER_RESOURCE_COUNT; ++ix) {
                    for(int ix_img=0; ix_img < sizeof(wdgt->sub.slider.res[ix].image_paths)/sizeof(wdgt->sub.slider.res[ix].image_paths[0]); ++ix_img) {
                        if (wdgt->sub.slider.res[ix].image_paths[ix_img]) {
                            wdgt->sub.slider.res[ix].textures[ix_img] = IMG_LoadTexture(wdgt->view->app->renderer, wdgt->sub.slider.res[ix].image_paths[ix_img]);
                            if (wdgt->sub.slider.res[ix].textures[ix_img] == NULL) {
                                 error_printf("widget_load_media: slider failed to load %d %s\n", ix, wdgt->sub.slider.res[ix].image_paths[ix_img]);
                            }
                        }
                    }
                }
                break;
        }
    }
    return wdgt;
}

widget* widget_rect(widget *wdgt, const SDL_Rect *rect) {
    if (wdgt) {
        copyRect(rect, &wdgt->rect);
        copyRect(rect, &wdgt->input_rect);
/*        
        if (wdgt->type == WIDGET_SLIDER) {
            wdgt->input_rect.y = wdgt->rect.y + wdgt->rect.h/3;
            wdgt->input_rect.h = wdgt->rect.h/3;
        }
*/
//        copyRect(&wdgt->rect, &wdgt->image_rect);
//        translate_image_rect(&wdgt->image_rect);

//        copyRect(&wdgt->rect, &wdgt->draw_rect);
//        translate_draw_rect(&wdgt->draw_rect);
    }
    return wdgt;
}

widget* widget_bounds(widget *wdgt, int x, int y, int w, int h) {
    SDL_Rect rect = {.x=x, .y=y, .w=w, .h=h};
    return widget_rect(wdgt, &rect);
}

/*
widget* widget_next(widget *wdgt, widget* next) {
    if (wdgt) {
        wdgt->next = next;
    }
    return wdgt;
}

widget* widget_prev(widget *wdgt, widget* prev) {
    if (wdgt) {
        if (prev) {
            wdgt->next = prev->next;
            prev->next = wdgt;
        }
    }
    return wdgt;
}
*/

widget* widget_action(widget* wdgt, action action) {
    if (wdgt) {
        if (wdgt->type != WIDGET_MULTISTATE_BUTTON) {
            wdgt->action = action;
        } else {
            if (action != ACTION_NONE) {
                error_printf("widget_action: ignoring set action for multistate button %p %d\n", wdgt, action);
            }
        }
    }
    return wdgt;
}

widget* widget_hide(widget* wdgt, bool hide) {
    if (wdgt) {
        wdgt->hidden = hide;
    }
    return wdgt;
}

widget* widget_image_path(widget* wdgt, const char* path) {
    if (wdgt) {
        if (wdgt->image_path != NULL) {
            free((void *)wdgt->image_path);
            wdgt->image_path = NULL;
        }
        if (path) {
            wdgt->image_path = strdup(path);
        }
    }
    return wdgt;
}

widget* widget_focus_enable(widget* wdgt, bool f) {
    *((bool *)(&wdgt->focus_disabled)) = !f;
    return wdgt;
}

static widget* widget_create(const view_context *view) {
    widget* wdgt = calloc(sizeof(*wdgt), 1);
    if (wdgt) {
        wdgt->view = view;
        wdgt->action = ACTION_NONE;
        wdgt->render = render_none;
        if (view->list) {
            wdgt->next = &view->list->tail;
            wdgt->prev = view->list->tail.prev;
            wdgt->prev->next = wdgt->next->prev = wdgt;
        }
    }
    return wdgt;
}

widget* widget_destroy(widget* wdgt) {
    if (wdgt) {
        switch(wdgt->type) {
            case WIDGET_NONE:
            case WIDGET_END:
                break;
            case WIDGET_VUMETER:
                vumeter_widget_destroy(wdgt);
                break;
            case WIDGET_IMAGE:
                break;
            case WIDGET_BUTTON:
                break;
            case WIDGET_MULTISTATE_BUTTON:
                {
                    _btn_resource* res =  wdgt->sub.multistate_button.res;
                    for(int ims=0; ims < wdgt->sub.multistate_button.state_count; ++ims) {
                        free((void *)res[ims].resource_path);
                    }
                    free(res);
                    wdgt->sub.multistate_button.res = NULL;
                }break;
            case WIDGET_SLIDER:
                for(int ix=0; ix<SLIDER_RESOURCE_COUNT; ++ix) {
                    for(int ix_txtr=0; ix_txtr < sizeof(wdgt->sub.slider.res[ix].image_paths)/sizeof(wdgt->sub.slider.res[ix].image_paths[0]); ++ix_txtr) {
                        if ( wdgt->sub.slider.res[ix].textures[ix_txtr] ) {
                            SDL_DestroyTexture(wdgt->sub.slider.res[ix].textures[ix_txtr]);
                            wdgt->sub.slider.res[ix].textures[ix_txtr] = NULL;
                        }
                    }
                    for(int ix_img=0; ix_img < sizeof(wdgt->sub.slider.res[ix].image_paths)/sizeof(wdgt->sub.slider.res[ix].image_paths[0]); ++ix_img) {
                        if ( wdgt->sub.slider.res[ix].image_paths[ix_img] ) {
                            free((void *)wdgt->sub.slider.res[ix].image_paths[ix_img]);
                            wdgt->sub.slider.res[ix].image_paths[ix_img] = NULL;
                        }
                    }
                }
                break;
        }
        if (wdgt->image_path != NULL) {
            free((void *)wdgt->image_path);
        }
        free(wdgt);
        wdgt = NULL;
    }
    return wdgt;
}

widget* widget_create_button(const view_context* view) {
    widget* wdgt = widget_create(view);
    if (wdgt) {
        *((widget_type*)&wdgt->type) = WIDGET_BUTTON;
        wdgt->action = ACTION_NONE;
        wdgt->render = button_widget_render;
    }
    return wdgt;
}

static void image_widget_render(widget* wdgt) {
    if (wdgt->highlight) {
        if (show_rects) { _show_draw_rect(wdgt); }
        if (show_input_rects) { _show_input_rect(wdgt); }
    }
    SDL_Rect image_rect;
    copyRect(&wdgt->rect, &image_rect);
    translate_image_rect(&image_rect);

    switch(wdgt->sub.image.scale_op) {
        case IMAGE_STRETCH_FILL:
            SDL_RenderCopyEx(wdgt->view->app->renderer, wdgt->sub.image.texture, NULL,
                &image_rect, wdgt->view->app->orientation, NULL, flip);
            break;
        case IMAGE_FIT:
            SDL_RenderCopyEx(wdgt->view->app->renderer, wdgt->sub.image.texture, NULL,
                &wdgt->sub.image.dst_rect, wdgt->view->app->orientation, NULL, flip);
            break;
        case IMAGE_CENTRED_FILL:
            SDL_RenderCopyEx(wdgt->view->app->renderer, wdgt->sub.image.texture, &wdgt->sub.image.src_rect,
                &image_rect, wdgt->view->app->orientation, NULL, flip);
            break;
    }
}

widget* widget_create_image(const view_context* view) {
    widget* wdgt = widget_create(view);
    if (wdgt) {
        *((widget_type*)&wdgt->type) = WIDGET_IMAGE;
        wdgt->action = ACTION_NONE;
        wdgt->render = image_widget_render;
    }
    return wdgt;
}

widget* widget_image_scaling(widget* wdgt, image_scaling op) {
    wdgt->sub.image.scale_op = op;
    setup_image_fit_src_rect(wdgt);
    return wdgt;
}

widget* widget_hotspot_edge(widget* wdgt, hotspot_edge edge, SDL_Rect *r) {
    if (r == NULL) {
        switch(edge) {
            case EDGE_NONE:
                break;
            case EDGE_LEFT:
            case EDGE_RIGHT:
                wdgt->input_rect.y = 0;
                wdgt->input_rect.h = 10000;
                break;
            case EDGE_TOP:
            case EDGE_BOTTOM:
                wdgt->input_rect.x = 0;
                wdgt->input_rect.w = 10000;
                break;
        }
    } else {
        SDL_Rect rect;
        copyRect(r, &rect);
        switch(edge) {
            case EDGE_NONE:
                break;
            case EDGE_LEFT:
                wdgt->input_rect.y = rect.y;
                wdgt->input_rect.h = rect.h;
                break;
            case EDGE_RIGHT:
                wdgt->input_rect.y = rect.y;
                wdgt->input_rect.h = rect.h;
                break;
            case EDGE_TOP:
                wdgt->input_rect.x = rect.x;
                wdgt->input_rect.w = rect.w;
                break;
            case EDGE_BOTTOM:
                wdgt->input_rect.x = rect.x;
                wdgt->input_rect.w = rect.w;
                break;
        }
    }
    return wdgt;
}

static void multistate_button_widget_render(widget* wdgt) {
    if (wdgt->pressed && !wdgt->hotspot) {
    SDL_Rect draw_rect;
        copyRect(&wdgt->rect, &draw_rect);
        translate_draw_rect(&draw_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 128, 128, 128, 16);
        SDL_RenderFillRect(wdgt->view->app->renderer, &draw_rect);
        SDL_SetRenderDrawColor(wdgt->view->app->renderer, 0, 0, 0, 0);
    }
    if (wdgt->highlight && wdgt->hotspot == false && show_rects) {
        _show_draw_rect(wdgt);
    }
    if (wdgt->highlight && show_input_rects) {
        _show_input_rect(wdgt);
    }
    if (wdgt->hotspot == false || wdgt->highlight)  {
        SDL_Rect image_rect;
        copyRect(&wdgt->rect, &image_rect);
        translate_image_rect(&image_rect);
        SDL_RenderCopyEx(wdgt->view->app->renderer,
            wdgt->sub.multistate_button.res[wdgt->sub.multistate_button.state].texture, NULL,
            &image_rect, wdgt->view->app->orientation, NULL, flip);
    }
}

widget* widget_create_multistate_button(const view_context* view, int state_count){
    widget* wdgt = widget_create(view);
    if (wdgt) {
        _btn_resource* res = calloc(state_count, sizeof(_btn_resource));
        if (res == NULL) {
            widget_destroy(wdgt);
            return NULL;
        }
        *((widget_type*)&wdgt->type) = WIDGET_MULTISTATE_BUTTON ;
        wdgt->sub.multistate_button.state_count = state_count;
        wdgt->sub.multistate_button.res = res;
        wdgt->action = ACTION_MULTISTATE_BUTTON;
        wdgt->render = multistate_button_widget_render;
    }
    return wdgt;
}

widget* widget_multistate_button_addstate(widget* wdgt, unsigned statenum, const char* image_name, action action) {
    if (wdgt->type == WIDGET_MULTISTATE_BUTTON && statenum <  wdgt->sub.multistate_button.state_count) {
        _btn_resource* res = wdgt->sub.multistate_button.res + statenum;
        if (res->resource_path) {
            free((void*)res->resource_path);
            res->resource_path = NULL;
        }
        if (res->texture) {
            SDL_DestroyTexture(res->texture);
            res->texture = NULL;
        }
        res->resource_path = strdup(image_name);
        res->action = action;
    }
    return wdgt;
}

widget* widget_multistate_button_set_state(widget* wdgt, unsigned statenum) {
    if (wdgt->type == WIDGET_MULTISTATE_BUTTON && statenum) {
        if (wdgt->sub.multistate_button.state_count > statenum) {
            wdgt->sub.multistate_button.state = statenum;
        }
    }
    return wdgt;
}

widget* widget_multistate_button_get_state(widget* wdgt, unsigned* statenum) {
    if (wdgt->type == WIDGET_MULTISTATE_BUTTON && statenum) {
        *statenum = wdgt->sub.multistate_button.state;
    }
    return wdgt;
}

widget* widget_hotspot(widget* wdgt, bool hotspot) {
    wdgt->hotspot = hotspot;
    return wdgt;
}

static _slider_workspace* slider_widget_init_workspace(widget* wdgt) {
#define ZAP_RECT(r) (r).x = (r).y = (r).w = (r).h = 0

    if (!wdgt->sub.slider.wk.initialised) {
        _slider_resource* pick = wdgt->sub.slider.res+SLIDER_PICK;
        
        _slider_workspace* wk = &wdgt->sub.slider.wk;
        int bar_w = wdgt->rect.w;
        int bar_x = wdgt->rect.x;

        ZAP_RECT(wk->bar_start_rect);
        ZAP_RECT(wk->bar_end_rect);
        ZAP_RECT(wk->bar_rect);

        wk->value_range_delta = wdgt->sub.slider.range.end - wdgt->sub.slider.range.start;
        wk->half_pw = pick? pick->w/2 : 0;
        wk->current_pos = wk->min_pos = wdgt->rect.x + wk->half_pw;
        wk->max_pos = wdgt->rect.x + wdgt->rect.w - wk->half_pw;
        dummy_printf("pick      : %4d %4d\n", pick? pick->w : -1, wk->half_pw);
        dummy_printf("x-extents : %4d %4d\n", wdgt->rect.x, wdgt->rect.x + wdgt->rect.w -1);
        dummy_printf("pos       : %4d %4d %4d\n", wk->min_pos, wk->max_pos, wk->max_pos - wk->min_pos);

        {
            _slider_resource* bar_start = wdgt->sub.slider.res[SLIDER_BAR_START].textures[0]? wdgt->sub.slider.res+SLIDER_BAR_START:NULL;
            SDL_Rect* r = &wk->bar_start_rect;
            if(bar_start) {
                r->w = bar_start->w;
                r->h = wdgt->rect.h;
                r->x = wdgt->rect.x;
                r->y = wdgt->rect.y;
                bar_w -= bar_start->w;
                bar_x += bar_start->w;
                translate_image_rect(&wk->bar_start_rect);
            }
        }
        {
            _slider_resource* bar_end = wdgt->sub.slider.res[SLIDER_BAR_END].textures[0]? wdgt->sub.slider.res+SLIDER_BAR_END:NULL;
            SDL_Rect* r = &wk->bar_end_rect;
            if(bar_end) {
                r->w = bar_end->w;
                r->h = wdgt->rect.h;
                r->x = wdgt->rect.x + wdgt->rect.w - bar_end->w -1;
                r->y = wdgt->rect.y;
                bar_w -= bar_end->w;
                translate_image_rect(&wk->bar_end_rect);
            }
        }
        {
            _slider_resource* bar = wdgt->sub.slider.res[SLIDER_BAR].textures[0]? wdgt->sub.slider.res+SLIDER_BAR:NULL;
            SDL_Rect* r = &wk->bar_rect;
            if (bar) {
                r->w = bar_w - 1;
                r->h = wdgt->rect.h;
                r->x = bar_x;
                r->y = wdgt->rect.y;
            }
        }
        if (pick && pick->h >0) {
            wdgt->input_rect.y = wdgt->rect.y + (wdgt->rect.h-pick->h)/2;
            wdgt->input_rect.h = pick->h;
        }
        wdgt->sub.slider.wk.initialised = true;
    }
    return &wdgt->sub.slider.wk;
}

static void slider_widget_render(widget* wdgt) {
    if (wdgt->highlight) {
        if (show_rects) { _show_draw_rect(wdgt); }
        if (show_input_rects) { _show_input_rect(wdgt); }
    }
    _slider_workspace* wk = slider_widget_init_workspace(wdgt);
/*    
    if (wk == NULL) {
        return;
    }
*/
    _slider_resource* pick = wdgt->sub.slider.res+SLIDER_PICK;

    SDL_Rect pick_rect;
    copyRect(&wdgt->rect, &pick_rect);
    if (wk->value_range_delta > 0) {
        if (wdgt->pressed) {
            pick_rect.x = wk->drag_pos - wk->half_pw;
        } else {
            pick_rect.x = wk->current_pos - wk->half_pw;
        }
        pick_rect.w = pick->w;
    }

    {
        _slider_resource* bar_start = wdgt->sub.slider.res[SLIDER_BAR_START].textures[0]? wdgt->sub.slider.res+SLIDER_BAR_START:NULL;
        if (bar_start) {
            int ix_texture = wk->current_pos > wk->min_pos? 1: 0;
            SDL_RenderCopyEx(wdgt->view->app->renderer, bar_start->textures[ix_texture], NULL,
                &wk->bar_start_rect, wdgt->view->app->orientation, NULL, flip);
        }
    }

    {
        _slider_resource* bar_end = wdgt->sub.slider.res[SLIDER_BAR_END].textures[0]? wdgt->sub.slider.res+SLIDER_BAR_END:NULL;
        if (bar_end) {
            int ix_texture = wk->current_pos < wk->max_pos? 0: 1;
            SDL_RenderCopyEx(wdgt->view->app->renderer, bar_end->textures[ix_texture], NULL,
                &wk->bar_end_rect, wdgt->view->app->orientation, NULL, flip);
        }
    }

    _slider_resource* bar = wdgt->sub.slider.res[SLIDER_BAR].textures[0]? wdgt->sub.slider.res+SLIDER_BAR:NULL;
    if (bar) {
        SDL_Rect image_rect;
        copyRect(&wk->bar_rect, &image_rect);
        image_rect.w = pick_rect.x - image_rect.x;
        translate_image_rect(&image_rect);
        SDL_RenderCopyEx(wdgt->view->app->renderer, bar->textures[0], NULL,
            &image_rect, wdgt->view->app->orientation, NULL, flip);
    }

    {
        SDL_Rect image_rect;
        copyRect( &pick_rect, &image_rect);
        translate_image_rect(&image_rect);
        SDL_RenderCopyEx(wdgt->view->app->renderer, pick->textures[0], NULL,
            &image_rect, wdgt->view->app->orientation, NULL, flip);
    }

    if (bar) {
        SDL_Rect image_rect;
        copyRect(&wk->bar_rect, &image_rect);
        image_rect.w -= pick_rect.x + pick_rect.w - image_rect.x;
        image_rect.x = pick_rect.x + pick_rect.w;
        translate_image_rect(&image_rect);
        SDL_RenderCopyEx(wdgt->view->app->renderer, bar->textures[1], NULL,
            &image_rect, wdgt->view->app->orientation, NULL, flip);
    }

}

widget *widget_create_slider(const view_context* view) {
    widget* wdgt = widget_create(view);
    if (wdgt) {
        *((widget_type*)&wdgt->type) = WIDGET_SLIDER;
        wdgt->action = ACTION_NONE;
        wdgt->render = slider_widget_render;
    }
    return wdgt;
}

widget *widget_slider_image_paths(widget* wdgt, slider_resource_ID id, const char* path1, const char* path2) {
    if (wdgt) {
        switch(id) {
            case SLIDER_BAR:
            case SLIDER_PICK:
            case SLIDER_BAR_END:
            case SLIDER_BAR_START:
                for(int ix=0; ix < sizeof(wdgt->sub.slider.res[id].image_paths)/sizeof(wdgt->sub.slider.res[id].image_paths[0]); ++ix) {
                    if (wdgt->sub.slider.res[id].image_paths[ix] != NULL) {
                        free((void *)wdgt->sub.slider.res[id].image_paths[ix]);
                        wdgt->sub.slider.res[id].image_paths[ix] = NULL;
                    }
                }
                if (path1) {
                    wdgt->sub.slider.res[id].image_paths[0] = strdup(path1);
                }
                if (path2) {
                    wdgt->sub.slider.res[id].image_paths[1] = strdup(path2);
                }
                break;
            case SLIDER_RESOURCE_COUNT:
            default:
                error_printf("widget_slider_image_path: unknown resource id %d\n", id);
                break;
        }
        wdgt->sub.slider.wk.initialised = false;
    }
    return wdgt;
}


widget *widget_slider_image_width(widget* wdgt, slider_resource_ID id, int width) {
    if (wdgt) {
        switch(id) {
            case SLIDER_PICK:
                wdgt->sub.slider.res[id].w = MAX(width, 2);
                break;
            case SLIDER_BAR_END:
            case SLIDER_BAR_START:
                wdgt->sub.slider.res[id].w = width;
                break;
            case SLIDER_BAR:
                error_printf("widget_slider_image_width: ignoring width setting for slider bar resource id %d\n", id);
                break;
            case SLIDER_RESOURCE_COUNT:
            default:
                error_printf("widget_slider_image_width: unknown resource id %d\n", id);
                break;
        }
        wdgt->sub.slider.wk.initialised = false;
    }
    return wdgt;
}

widget *widget_slider_image_height(widget* wdgt, slider_resource_ID id, int height) {
    if (wdgt) {
        switch(id) {
            case SLIDER_PICK:
            case SLIDER_BAR_END:
            case SLIDER_BAR_START:
            case SLIDER_BAR:
                wdgt->sub.slider.res[id].h = height;
                break;
            case SLIDER_RESOURCE_COUNT:
            default:
                error_printf("widget_slider_image_height: unknown resource id %d\n", id);
                break;
        }
        wdgt->sub.slider.wk.initialised = false;
    }
    return wdgt;
}

static widget *widget_slider_track(widget* wdgt, const SDL_Point *pt) {
    if (wdgt->pressed && (wdgt->sub.slider.range.end - wdgt->sub.slider.range.start) > 0) {
        _slider_resource* pick = wdgt->sub.slider.res+SLIDER_PICK;
        _slider_workspace* wk = &wdgt->sub.slider.wk;
        if (pick) {
            if (pt->x < wk->min_pos) {
                wk->drag_pos = wk->min_pos;
            } else if (pt->x > wk->max_pos) {
                wk->drag_pos = wk->max_pos;
            } else {
                wk->drag_pos = pt->x;
            }
        }
    }
    return wdgt;
}

static widget *widget_slider_tracking_commit(widget* wdgt, const SDL_Point *pt) {
    widget_slider_track(wdgt, pt);
    _slider_workspace* wk = &wdgt->sub.slider.wk;
    wk->current_pos = wk->drag_pos;
    return wdgt;
}


widget *widget_slider_set_value(widget* wdgt, int value) {
    if (wdgt && wdgt->type == WIDGET_SLIDER) {
        if (value >= wdgt->sub.slider.range.start && value <= wdgt->sub.slider.range.end) {
            _slider_workspace* wk = slider_widget_init_workspace(wdgt);
            if (wk->value_range_delta) {
                // range must be non-zero to calculate the position of the pick
                wk->current_pos = ((value - wdgt->sub.slider.range.start)*(wk->max_pos - wk->min_pos))/wk->value_range_delta;
                dummy_printf("widget_slider_set_value (%d * %d)/%d = %d, for %d\n", 
                        value - wdgt->sub.slider.range.start,
                        (wk->max_pos - wk->min_pos),
                        wk->value_range_delta,
                        wk->current_pos,
                        value);
            }
        } else {
            error_printf("widget_slider_set_value: %d not in range %d-%d\n",
                    value,
                    wdgt->sub.slider.range.start,
                    wdgt->sub.slider.range.end);
        }
    }
    return wdgt;
}

widget *widget_slider_range(widget* wdgt, int start, int end) {
    if (wdgt && wdgt->type == WIDGET_SLIDER) {
        wdgt->sub.slider.range.start = start;
        wdgt->sub.slider.range.end = end;
        // workspace needs reinitialising - set the flag
        wdgt->sub.slider.wk.initialised = false;
        // widget_slider_set_value will initialise the workspace
        widget_slider_set_value(wdgt, wdgt->sub.slider.range.start);
    }
    return wdgt;
}


widget *widget_slider_get_value(widget* wdgt, int* value) {
    if (wdgt && wdgt->type == WIDGET_SLIDER) {
        _slider_workspace* wk = &wdgt->sub.slider.wk;
        int v = (wk->current_pos - wk->min_pos);
        dummy_printf("widget_slider_get_value v=%d\n", v);
        v *= wk->value_range_delta;
        v /= (wk->max_pos - wk->min_pos);
        *value = wdgt->sub.slider.range.start + v;
    }
    return wdgt;
}


static widget_list* widget_list_initialise(widget_list* list, view_context* view) {
    if (list) {
        list->head.next = &list->tail;
        *((widget_type*)(&list->head.type)) = WIDGET_NONE;
        list->tail.prev = &list->head;
        *((widget_type*)(&list->tail.type)) = WIDGET_END;
        for(widget* w=&list->head; w != NULL; w=w->next) {
            w->view = view;
            w->hidden = true;
            *((bool *)(&w->focus_disabled)) = true;
            w->rect.x = w->rect.y =  -100000;
            w->rect.w = w->rect.h =  0;
            w->input_rect.x = w->input_rect.y =  -100000;
            w->input_rect.w = w->input_rect.h =  0;
            w->render = render_none;
        }
    }
    return list;
}

widget_list* create_widget_list(view_context* view) {
    return widget_list_initialise(calloc(sizeof(widget_list), 1), view);
}

widget_list* destroy_widgets_in_list(widget_list* list) {
    if (list) {
        widget* widget = list->head.next;
        while(widget != &list->tail) {
            list->head.next = widget->next;
            widget_destroy(widget);
            widget = list->head.next;
        }
        list->head.next = &list->tail;
        list->tail.prev = &list->head;
        return list;
    }
    return list;
}

widget_list* destroy_widget_list(widget_list* list) {
    if (list) {
        destroy_widgets_in_list(list);
        free(list);
    }
    return NULL;
}

void widget_list_load_media(const widget_list* list, const char* resource_path) {
    for (widget* widget = list->head.next; widget != NULL; widget = widget->next) {
        widget_load_media(widget, resource_path);
    }
}

void widget_list_react(const widget_list* list, const pointer_input input, SDL_Point* pt) {
    bool selected = false;
    input_printf("%d: %04d,%04d -> ", input, pt->x, pt->y);
    translate_point(pt);
    input_printf(" %04d,%04d\n", pt->x, pt->y);
    switch(input) {
        case POINTER_DOWN:
            for(widget* widget=list->tail.prev; widget != NULL; widget=widget->prev) {
                if (widget->hidden) { continue;}
                if (!selected) {
                    widget->focussed = SDL_PointInRect(pt, &widget->input_rect) && (!widget->focus_disabled);
                    widget->highlight = widget->focussed;
                    widget->pressed = widget->focussed;
                    selected = widget->focussed;
                    if (widget->type == WIDGET_SLIDER) {
                        widget_slider_track(widget, pt);
                    }
                } else {
                    widget->focussed = false;
                    widget->highlight = false;
                    widget->pressed = false;
                }
            }
            break;
        case POINTER_UP:
            for(widget* widget=list->tail.prev; widget != NULL; widget=widget->prev) {
                if (widget->hidden) { continue;}
                widget->pressed = false;
                if (SDL_PointInRect(pt, &widget->input_rect) && widget->focussed) {
                   input_printf("HIT: {%04d,%04d} {%04d,%04d} %p\n",
                                widget->input_rect.x, widget->input_rect.y,
                                widget->input_rect.x + widget->input_rect.w,
                                widget->input_rect.y, widget->input_rect.h,
                                widget
                                );
                    widget->focussed = false;
                    widget->highlight = widget->focussed;
                    if (widget->type == WIDGET_SLIDER) {
                        widget_slider_tracking_commit(widget, pt);
                        int value =  -987654321;
                        widget_slider_get_value(widget, &value);
                        dummy_printf("slider value = %d\n", value);
                    }
                    widget_dispatch_action(widget);
                } else {
                    widget->focussed = false;
                    widget->highlight = false;
                }
            }
            break;
        case POINTER_MOTION:
            for (widget* widget=list->tail.prev; widget != NULL; widget=widget->prev) {
                if (widget->hidden) { continue;}
                if (!selected) {
                    widget->highlight = SDL_PointInRect(pt, &widget->input_rect) && (!widget->focus_disabled);
                    selected = widget->highlight;
                    if (selected) {
                        if (widget->type == WIDGET_SLIDER) {
                            widget_slider_track(widget, pt);
                        }
                    }
                } else {
                    widget->highlight = false;
                }
            }
            break;
    }
}

