#ifndef __CONTROL_H_
#define __CONTROL_H_
#include <SDL2/SDL.h>
#include "util.h"
#include "actions.h"
#include "types.h"
#include "texture_cache.h"

#define  WH_FILL (-1)

typedef enum {
    WIDGET_NONE,
    WIDGET_IMAGE,
    WIDGET_BUTTON,
    WIDGET_MULTISTATE_BUTTON,
    WIDGET_VUMETER,
    WIDGET_SLIDER,
    WIDGET_END
}widget_type;
  

typedef enum {
    USEREVENT_NEXT_VISU = SDL_USEREVENT +1,
    USEREVENT_PREV_VISU,
    USEREVENT_NEXT_VU,
    USEREVENT_PREV_VU,
    USEREVENT_NEXT_SP,
    USEREVENT_PREV_SP,
}user_event;

typedef enum {
    IMAGE_FIT,
    IMAGE_CENTRED_FILL,
    IMAGE_STRETCH_FILL,
}image_scaling;

typedef enum {
    EDGE_NONE,
    EDGE_LEFT,
    EDGE_TOP,
    EDGE_RIGHT,
    EDGE_BOTTOM,
}hotspot_edge;

typedef enum {
    POINTER_DOWN,
    POINTER_UP,
    POINTER_MOTION
} pointer_input;

typedef struct vumeter_widget vumeter_widget;
typedef struct spmeter_widget spmeter_widget;

typedef struct widget widget;
typedef struct widget_list widget_list;
typedef struct view_context view_context;

typedef struct _btn_resource {
    texture_id_t    texture_id;
    const char      *resource_path;
    action          action;
}_btn_resource;

typedef struct _slider_resource {
    const char*     image_paths[2];
    texture_id_t    texture_ids[2];
    int w;
    int h;
}_slider_resource;
    
typedef enum {
    SLIDER_BAR,
    SLIDER_PICK,
    SLIDER_BAR_START,
    SLIDER_BAR_END,
    SLIDER_RESOURCE_COUNT
}slider_resource_ID;

typedef struct {
    bool initialised;
    int  value_range_delta;
    int  half_pw;
    int  min_pos;
    int  max_pos;
    int  current_pos;
    int  drag_pos;
    SDL_Rect bar_start_rect;
    SDL_Rect bar_end_rect;
    SDL_Rect bar_rect;
    SDL_Rect bar_empty_rect;
}_slider_workspace;

struct widget {
    struct      widget *next;
    struct      widget *prev;
    const       widget_type type;
    const       view_context* view;
    
//    void        (*action)(widget* wdgt);
    action         action;
    void        (*render)(struct widget*);
    bool        focussed;
    bool        highlight;
    bool        hidden;
    bool        hotspot;
    const bool  focus_disabled;
    // generic image path for all widgets with single images
    // can be NULL
    const char* image_path;
    
    SDL_Rect    rect;
    SDL_Rect    input_rect;
    // For now 2 translated rectangles are used to handle 
    // orientation correctly. 
    // TODO further investigation.
    // 1 - for rotated images
    // SDL_Rect    image_rect;
    // 2 - for unrotated operations like DrawRect, FillRect
    //SDL_Rect    draw_rect;

    bool         pressed;

    union {
        vumeter_widget* vu;
        spmeter_widget* sp;
        struct {
            texture_id_t texture_id;
            int w;
            int h;
            image_scaling scale_op;
            SDL_Rect src_rect;
            SDL_Rect dst_rect;
        }image;
        struct {
            texture_id_t texture_id;
        }button;
        struct {
            unsigned state;
            unsigned state_count;
            _btn_resource* res;
        }multistate_button;
        struct {
            bool interactive;
            struct {
                int start;
                int end;
            }range;
            _slider_resource res[SLIDER_RESOURCE_COUNT];
            _slider_workspace wk;
        }slider;
    }sub;
};

extern bool show_rects;
extern bool show_input_rects;

void render_none(widget* wdgt);

const char* widget_type_name(widget_type typ);
widget* widget_rect(widget *wdgt, const SDL_Rect *rect);
widget* widget_bounds(widget *wdgt, int x, int y, int w, int h);

/*
widget* widget_next(widget *wdgt, widget* next);
widget* widget_prev(widget *wdgt, widget* prev);
*/
widget* widget_load_media(widget* wdgt, const char* resource_path);
widget* widget_destroy(widget* wdgt);
widget* widget_action(widget* wdgt, action action);
widget* widget_hide(widget* wdgt, bool hide);
widget* widget_hotspot(widget* wdgt, bool hotspot);
widget* widget_hotspot_edge(widget* wdgt, hotspot_edge edge, SDL_Rect *r);
widget* widget_image_path(widget* wdgt, const char* path);
widget* widget_focus_enable(widget* wdgt, bool f);

widget* widget_create_button(const view_context*);
widget* widget_create_multistate_button(const view_context*, int state_count);
widget* widget_multistate_button_addstate(widget* wdgt, unsigned statenum, const char* resource_path, action);
widget* widget_multistate_button_set_state(widget* wdgt, unsigned statenum);
widget* widget_multistate_button_get_state(widget* wdgt, unsigned* statenum);

widget* widget_create_image(const view_context*);
widget* widget_image_scaling(widget *wdgt, image_scaling op);

widget *widget_create_vumeter(const view_context*);
widget *widget_vumeter_select_next(widget *wdgt);
widget *widget_vumeter_select_prev(widget *wdgt);
widget *widget_vumeter_select_by_name(widget *wdgt, const char* name);
widget *widget_vumeter_select_lock(widget *wdgt, bool lock);

widget *widget_create_slider(const view_context*);
widget *widget_slider_range(widget* , int start, int end);
widget *widget_slider_set_value(widget* wdgt, int value);
widget *widget_slider_get_value(widget* wdgt, int* value);
widget *widget_slider_image_paths(widget* , slider_resource_ID id, const char* path1, const char* path2);
widget *widget_slider_image_width(widget* , slider_resource_ID id, int width);
widget *widget_slider_image_height(widget* , slider_resource_ID id, int height);

struct widget_list {
    widget head;
    widget tail;
};

struct view_context {
    const app_context* app;
    widget_list*       list;
};

widget_list* create_widget_list(view_context* view);
widget_list* destroy_widget_list(widget_list*);
widget_list* destroy_widgets_in_list(widget_list*);

void widget_dispatch_action(widget* wdgt);
void widget_list_load_media(const widget_list* list, const char* resource_path);
void widget_list_react(const widget_list* list, const pointer_input input, SDL_Point* pt);
#endif
