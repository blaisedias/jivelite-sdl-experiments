#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include "widgets.h"
#include "actions.h"
#include "logging.h"
#include "util.h"

#define bool  SDL_bool
float scalef = 1.0;

static const char* widget_type_strings[] = {
    "",
    "image",
    "button",
    "multistate_button",
    "vumeter",
    "slider",
    ""
};

static widget_type tokenise_widget(const char* str) {
    if (str == NULL) {
        return WIDGET_NONE;
    }
    for(int x=0; x<sizeof(widget_type_strings)/sizeof(widget_type_strings[0]); ++x) {
        if (0 == strcmp(widget_type_strings[x], str)) {
            return x;
        }
    }
    return WIDGET_END;
}

typedef enum {
    JT_NONE,
    JT_SCREEN,
    JT_WIDGETS,

    JT_X,
    JT_Y,
    JT_W,
    JT_H,
    JT_LOCATION,
    JT_POSITION,
    JT_CONTAINER,

    JT_IMAGE,
    JT_IMAGE_SCALING,

    JT_BUTTON,
    JT_MULTISTATE_BUTTON,

    JT_ACTION,
    JT_STATE,
    JT_STATES,
    JT_HOTSPOT,
    JT_HOTSPOT_EDGE,
    JT_FOCUS_ENABLE,
    JT_HIDDEN,

    JT_SELECT,

    JT_RANGE,
    JT_RANGE_START,
    JT_RANGE_END,

// order of bar, pick, bar-start, bar-end should match slider_resource_ID enum
    JT_BAR,
    JT_PICK,
    JT_BAR_START,
    JT_BAR_END,
    JT_WIDTH,
    JT_HEIGHT,

    JT_END,

} json_token;

static const char* json_token_strings[]= {
    "",
    "screen",
    "widgets",

    "x",
    "y",
    "w",
    "h",
    "location",
    "position",
    "container",

    "image",
    "image_scaling",

    "button",
    "multistate_button",

    "action",
    "state",
    "states",
    "hotspot",
    "hotspot_edge",
    "focus_enable",
    "hidden",

    "select",

    "range",
    "start",
    "end",

    "bar",
    "pick",
    "bar_start",
    "bar_end",
    "width",
    "height",

    "",
};

static json_token tokenise(json_object_entry* joe) {
    for(int x=0; x < sizeof(json_token_strings)/sizeof(json_token_strings[0]); ++x) {
        if ( 0 == strcmp(json_token_strings[x], joe->name)) {
            return x;
        }
    }
    return JT_END;
}

static const char* edge_strings[] = {
    "",
    "left",
    "top",
    "right",
    "bottom"
};

static hotspot_edge tokenise_hotspot_edge(const char* str) {
    if (str == NULL) {
        return EDGE_NONE;
    }
    for(int x=0; x<sizeof(edge_strings)/sizeof(edge_strings[0]); ++x) {
        if (0 == strcmp(edge_strings[x], str)) {
            return x;
        }
    }
    return EDGE_NONE;
}


static const char* image_scaling_strings[] = {
    "fit",
    "center",
    "stretch",
};

static image_scaling tokenise_image_scaling(const char* str) {
    if (str == NULL) {
        return IMAGE_FIT;
    }
    for(int x=0; x<sizeof(image_scaling_strings)/sizeof(image_scaling_strings[0]); ++x) {
        if (0 == strcmp(image_scaling_strings[x], str)) {
            return x;
        }
    }
    return IMAGE_FIT;
}


static json_value* get_object_value(json_value* value, json_token jt) {
    if (value == NULL) {
        error_printf("get_object_value: value==NULL\n");
        return NULL;
    }
    if (value->type != json_object) {
        error_printf("get_object_value: != object\n");
        return NULL;
    }
    for (int x=0; x < value->u.object.length; x++) {
        if (jt == tokenise(value->u.object.values + x) ) {
            return value->u.object.values[x].value;
        }
    }
    return NULL;
}

static json_value* get_object_object_value(json_value* value, json_token jt) {
    value = get_object_value(value, jt);
    if (value && value->type == json_object) {
        return value;
    }
    return NULL;
}

static int get_object_int_value(json_value* value, json_token jt, int default_value) {
    value = get_object_value(value, jt);
    if (value && value->type == json_integer) {
        return value->u.integer;
    }
    return default_value;
}

static int get_scaled_object_int_value(json_value* value, json_token jt, int default_value) {
    value = get_object_value(value, jt);
    if (value && value->type == json_integer) {
        return (int)(value->u.integer*scalef);
    }
    return default_value;
}


static const char* get_object_string_value(json_value* value, json_token jt, const char* default_value) {
    value = get_object_value(value, jt);
    if (value && value->type == json_string) {
            return value->u.string.ptr;
    }
    return default_value;
}

static bool get_object_boolean_value(json_value* value, json_token jt, bool default_value) {
    value = get_object_value(value, jt);
    if (value && value->type == json_boolean) {
            return (bool)value->u.boolean;
    }
    return default_value;
}

/*
static double get_object_double_value(json_value* value, json_token jt, double default_value) {
    value = get_object_value(value, jt);
    if (value && value->type == json_double) {
            return value->u.dbl;
    }
    return default_value;
}
*/

static void deserialise_container(json_value* value, SDL_Rect* container, view_context* ctx) {
    if (value == NULL) {
//        error_printf("deserialise_container value==NULL\n");
        return;
    }
    if (value->type != json_object) {
        error_printf("deserialise_container: value != object\n");
        return;
    }
    container->x = get_scaled_object_int_value(value, JT_X, container->x);
    container->y = get_scaled_object_int_value(value, JT_Y, container->y);
    container->w = get_scaled_object_int_value(value, JT_W, container->w);
    if (container->w <= 0) {
        container->w = ctx->app->window_rect.w + container->w - container->x;
    }
    container->h = get_scaled_object_int_value(value, JT_H, container->h);
    if (container->h <= 0) {
        container->h = ctx->app->window_rect.h + container->h - container->y;
    }
}


typedef enum {
    P_NONE,
    P_TOP,
    P_BOTTOM,
    P_LEFT,
    P_RIGHT,
    P_CENTER,
    P_END
} position;

const char* position_strings[] = {
    "",
    "top",
    "bottom",
    "left",
    "right",
    "center",
    ""
};

static position tokenise_position(const char* str) {
    if (str) {
        for(int x=0; x < sizeof(position_strings)/sizeof(position_strings[0]); ++x) {
            if (0 == strcmp(str, position_strings[x])) {
                return x;
            }
        }
        return P_END;
    }
    return P_NONE;
}

static void deserialise_position(json_value* value, SDL_Rect* container, SDL_Rect* rect, view_context* ctx) {
    if (value == NULL) {
        error_printf("deserialise_position: value==NULL\n");
        return;
    }
    if (value->type != json_object) {
        error_printf("deserialise_position: value != object\n");
        return;
    }

    {
        const char* strX = get_object_string_value(value, JT_X, NULL);
        if (strX != NULL ) {
            if (get_object_string_value(value, JT_W, NULL) != NULL) {
                error_printf("deserialise_position: one of x or w must be an absolute value");
                rect->x = container->x;
                rect->w = container->w;
            } else {
                rect->w = get_scaled_object_int_value(value, JT_W, container->w);
                if (rect->w <= 0) {
                    //  zero or negative values for width => value is a margin from right edge
                    //  however since x position may be derived from the width,
                    //  it is not possible to set width correctly
                    //  so set width to container width - margin value 
                    rect->w += container->w;
                }
                switch(tokenise_position(strX)) {
                    case P_LEFT:
                        rect->x = container->x;
                        break;
                    case P_CENTER:
                        rect->x = container->x + (container->w - rect->w)/2;
                        break;
                    case P_RIGHT:
                        rect->x = container->x + container->w - rect->w -1;
                        break;
                    default:
                        error_printf("deserialise_position: invalid string value for x= %s\n", strX);
                        rect->x = container->x;
                        break;
                }
            }
        } else {
            rect->x = get_scaled_object_int_value(value, JT_X, 0);
            rect->w = get_scaled_object_int_value(value, JT_W, container->w);
            if (rect->x < 0) {
                //  negative values for x => value is a margin from container right edge
                rect->x += container->x + container->w - 1;
            } else {
                rect->x += container->x;
            }
            if (rect->w <= 0) {
                //  negative values for width => value is a margin from right edge
                rect->w += container->w - rect->x;
            }
        }
    }

    {
        const char* strY = get_object_string_value(value, JT_Y, NULL);
        if (strY != NULL) {
            if (get_object_string_value(value, JT_H, NULL) != NULL) {
                error_printf("deserialise_position: one of y or h must be an absolute value");
                rect->y = container->y;
                rect->h = container->h;
                return;
            } else {
                rect->h = get_scaled_object_int_value(value, JT_H, container->h);
                if (rect->h <= 0) {
                    //  zero or negative values for height => value is a margin from bottom edge
                    //  however since y position may be derived from the height,
                    //  it is not possible to set height correctly
                    //  so set height to container height - margin value 
                    rect->h += container->h;
                }
                switch(tokenise_position(strY)) {
                    case P_TOP:
                        rect->y = container->y;
                        break;
                    case P_CENTER:
                        rect->y = container->y + (container->h - rect->h)/2;
                        break;
                    case P_BOTTOM:
                        rect->y = container->y + container->h - rect->h -1;
                        break;
                    default:
                        error_printf("deserialise_position invalid string value for y= %s\n",strY);
                        rect->y = container->y;
                        break;
                }
            }
        } else {
            rect->y = get_scaled_object_int_value(value, JT_Y, 0);
            rect->h = get_scaled_object_int_value(value, JT_H, container->h);
            if (rect->y < 0) {
                //  negative values for y => value is a margin from container bottom edge
                rect->y += container->y + container->h - 1;
            } else {
                rect->y += container->y;
            }
            if (rect->h <= 0) {
                //  negative values for height => value is a margin from bottom edge
                rect->h += container->h - rect->y;
            }
        }
    }
}

static void deserialise_location(json_value* value, view_context* ctx, SDL_Rect* container, widget* widget) {
    if (value == NULL) {
        error_printf("deserialise_location value==NULL\n");
        return;
    }
    if (value->type != json_object) {
        error_printf("deserialise_location != object\n");
        return;
    }

    // Default container is the full screen
    container->x = ctx->app->window_rect.x;
    container->y = ctx->app->window_rect.y;
    container->w = ctx->app->window_rect.w;
    container->h = ctx->app->window_rect.h;

    SDL_Rect rect = {container->x, container->y, container->w, container->h};

    deserialise_container(get_object_value(value, JT_CONTAINER), container, ctx);
    deserialise_position(get_object_value(value, JT_POSITION), container, &rect, ctx);
    json_printf("             : rect %d,%d,%d,%d \n", rect.x, rect.y, rect.w, rect.h );
    widget_rect(widget, &rect);
}

static void deserialise_one_widget(json_value* value, view_context* ctx) {
    if (value == NULL) {
        error_printf("deserialise_one_widget value==NULL\n");
        return;
    }
    if (value->type != json_object) {
        error_printf("deserialise_one_widget widget != object\n");
        return;
    }
    if (value->u.object.length != 1) {
        error_printf("deserialise_one_widget invalid object length\n");
        exit(EXIT_FAILURE);
    }
    const char* widget_typename = value->u.object.values[0].name;
    widget_type wdgt_type = tokenise_widget(value->u.object.values[0].name);
    switch(wdgt_type) {
        case WIDGET_NONE:
            error_printf("deserialise_one_widget: widget none\n");
            return;
        case WIDGET_END:
            error_printf("deserialise_one_widget: (END) unknown widget %s\n", widget_typename);
            return;
        case WIDGET_IMAGE:
        case WIDGET_BUTTON:
        case WIDGET_MULTISTATE_BUTTON:
        case WIDGET_VUMETER:
        case WIDGET_SLIDER:
            break;
        default:
            error_printf("deserialise_one_widget: (default) unknown widget %s\n", widget_typename);
            return;
    }
    json_printf("%s\n", widget_typename);
    value = value->u.object.values[0].value;
    if (value->type != json_object) {
        error_printf("deserialise_one_widget value != object\n");
        return;
    }
    widget* widget = NULL;
    switch(wdgt_type) {
        case WIDGET_NONE:
        case WIDGET_END:
            break;
        case WIDGET_IMAGE:
            {
                widget = widget_create_image(ctx);
                widget_image_path(widget, get_object_string_value(value, JT_IMAGE, NULL));
                json_printf("     image    %s\n", widget->image_path);
                widget_image_scaling(widget, tokenise_image_scaling(get_object_string_value(value, JT_IMAGE_SCALING, "fit")));
            }break;
        case WIDGET_BUTTON:
            {
                widget = widget_create_button(ctx);
                widget_image_path(widget, get_object_string_value(value, JT_IMAGE, NULL));
                json_printf("     image    %s\n", widget->image_path);
            }break;
        case WIDGET_MULTISTATE_BUTTON:
            {
                json_value* jstates = get_object_value(value, JT_STATES);
                if (jstates != NULL && jstates->type == json_array) {
                    json_printf("     states\n");
                    widget = widget_create_multistate_button(ctx, jstates->u.array.length);
                    for(int x=0; x<jstates->u.array.length; ++x) {
                        json_value* svalue = jstates->u.array.values[x];
                        widget_multistate_button_addstate(widget, x, 
                                get_object_string_value(svalue, JT_IMAGE, NULL),
                                action_from_string(get_object_string_value(svalue, JT_ACTION, NULL)));
                        json_printf("              %d %s %s\n",
                                action_from_string(get_object_string_value(svalue, JT_ACTION, NULL)),
                                get_object_string_value(svalue, JT_ACTION, NULL),
                                get_object_string_value(svalue, JT_IMAGE, NULL));
                    }
                } else {
                    error_printf("states is either missing or not an array %p", jstates);
                }
            }break;
        case WIDGET_VUMETER:
            {
                widget = widget_create_vumeter(ctx);
                // TODO remove this feature?
                widget_vumeter_select_by_name(widget, get_object_string_value(value, JT_SELECT, NULL));
            }break;
        case WIDGET_SLIDER:
            {
                widget = widget_create_slider(ctx);
                {
                    json_value* jrange = get_object_object_value(value, JT_RANGE);
                    if (jrange) {
                        widget_slider_range(widget, 
                                get_object_int_value(jrange, JT_RANGE_START, 0),
                                get_object_int_value(jrange, JT_RANGE_END, 0)
                                );
                        json_printf("     range:%d %d\n",
                                get_object_int_value(jrange, JT_RANGE_START, 0),
                                get_object_int_value(jrange, JT_RANGE_END, 0)
                                );
                    }
                }
                json_printf("     resources:\n");
                for(slider_resource_ID resid = 0; resid < SLIDER_RESOURCE_COUNT; ++resid) {
                    json_value* jslider = get_object_object_value(value, JT_BAR+resid);
                    if (jslider) {
                        json_value* jimage = get_object_value(jslider, JT_IMAGE);
                        if (jimage && jimage->type == json_array) {
                            json_value* svalue = jimage->u.array.length >0? jimage->u.array.values[0]: NULL;
                            char* path1 = svalue? svalue->u.string.ptr: NULL;
                            svalue = jimage->u.array.length >1? jimage->u.array.values[1]: NULL;
                            char* path2 = svalue? svalue->u.string.ptr: NULL;
                            widget_slider_image_paths(widget, resid, path1, path2);
                            json_printf("              %d img=[%s,%s]\n", resid, path1, path2);
                        } else {
                            widget_slider_image_paths(widget, resid, 
                                    get_object_string_value(jslider, JT_IMAGE, NULL),
                                    get_object_string_value(jslider, JT_IMAGE, NULL)
                                    );
                            json_printf("              %d img=%s\n", resid, get_object_string_value(jslider, JT_IMAGE, NULL));
                        }
                        if (get_object_value(jslider, JT_WIDTH)) {
                            widget_slider_image_width(widget, resid, 
                                    get_scaled_object_int_value(jslider, JT_WIDTH, 0));
                            json_printf("              %d w=%d\n", resid, get_scaled_object_int_value(jslider, JT_WIDTH, 0));
                        }
                        widget_slider_image_height(widget, resid, 
                            get_scaled_object_int_value(jslider, JT_HEIGHT, 0));
                        json_printf("              %d h=%d\n", resid, get_scaled_object_int_value(jslider, JT_HEIGHT, 0));
                    }
                }
            }break;
    }
    if (widget) {
        json_printf("     location\n");
        SDL_Rect container = {  -1, -1, -10000, -10000 };
        deserialise_location(get_object_value(value, JT_LOCATION), ctx, &container, widget);

        widget_action(widget, action_from_string(get_object_string_value(value, JT_ACTION, NULL)));
        json_printf("     action: %d %s\n",
            action_from_string(get_object_string_value(value, JT_ACTION, NULL)),
            get_object_string_value(value, JT_ACTION, NULL));

        widget_hide(widget, get_object_boolean_value(value, JT_HIDDEN, false));
        json_printf("     hidden: %d\n", widget->hidden);
        widget_hotspot(widget, get_object_boolean_value(value, JT_HOTSPOT, false));
        widget_hotspot_edge(widget, tokenise_hotspot_edge(get_object_string_value(value, JT_HOTSPOT_EDGE, NULL)), &container);
        json_printf("     hotspot: %d edge=%d, %s\n",
               widget->hotspot,
               tokenise_hotspot_edge(get_object_string_value(value, JT_HOTSPOT_EDGE, NULL)),
               get_object_string_value(value, JT_HOTSPOT_EDGE, NULL));
    } else {
        error_printf("deserialise_one_widget NULL widget for type %d\n", wdgt_type);
    }
}

void deserialise_widgets(json_value* value, view_context* ctx) {
    if (value == NULL) {
        error_printf("deserialise_widgets value==NULL\n");
        return;
    }
    if (value->type != json_object) {
        error_printf("deserialise_widgets !object\n");
        return;
    }
    json_value* jwidgets = get_object_value(value, JT_WIDGETS);
    if (jwidgets && jwidgets->type == json_array) {
        for (int x = 0; x < jwidgets->u.array.length; ++x) {
            deserialise_one_widget(jwidgets->u.array.values[x], ctx);
        }
    } else {
        error_printf("deserialise_widgets widgets object is missing or not an array %p\n", jwidgets);
    }
}

bool deserialise_screen(json_value* value, view_context* ctx, SDL_Rect* rect) {
    if (value == NULL) {
        error_printf("deserialise_screen value==NULL\n");
        return false;
    }
    if (value->type != json_object) {
        error_printf("deserialise_screen !object\n");
        return false;
    }
    value = get_object_value(value, JT_SCREEN);
    if (value && value->type == json_object) {
        rect->x = get_object_int_value(value, JT_X, rect->x);
        rect->y = get_object_int_value(value, JT_Y, rect->x);
        rect->w = get_object_int_value(value, JT_W, rect->w);
        rect->h = get_object_int_value(value, JT_H, rect->h);
        return true;
    } else {
        error_printf("deserialise_value screen object is missing, defaulting to \n");
    }
    return false;
}

int deserialise_json(const char* json_string, const int len, view_context* ctx) {
    json_value* value = json_parse(json_string, len);

    if (value == NULL) {
        error_printf("deserialise_json: failed to parse json string\n");
        return EXIT_FAILURE;
    }

    json_printf("view {%d,%d,%d,%d}\n",
            ctx->app->window_rect.x,
            ctx->app->window_rect.y,
            ctx->app->window_rect.w,
            ctx->app->window_rect.h);
    SDL_Rect spec_rect = {0, 0, 800, 480};
    if ( !deserialise_screen(value, ctx, &spec_rect) ) {
        error_printf("screen object is missing, defaulting to {%d,%d,%d,%d}\n",
                spec_rect.x, spec_rect.y, spec_rect.w, spec_rect.h);
    }

    scalef = MIN(
            (float)ctx->app->window_rect.w/spec_rect.w,
            (float)ctx->app->window_rect.h/spec_rect.h
            );
    printf("scaling factor = %f\n", scalef);
    deserialise_widgets(value, ctx);
    return 0;
}

int deserialise_widgets_file(const char* filepath, view_context* ctx) {
    FILE *fp;
    struct stat filestatus;
    char* json_string;

    if ( stat(filepath, &filestatus) != 0) {
        error_printf("deserialise_widgets_file: file %s not found\n", filepath);
        return EXIT_FAILURE;
    }

    json_string =  calloc(filestatus.st_size, 1);
    if (json_string == NULL) {
        error_printf("deserialise_widgets_file: OOM %d %s \n", filestatus.st_size, filepath);
        return EXIT_FAILURE;
    }

    fp = fopen(filepath, "rt");
    if (fp == NULL) {
        free(json_string);
        error_printf("deserialise_widgets_file: failed to open file %s \n", filepath);
        return EXIT_FAILURE;
    }

    if (1 != fread(json_string, filestatus.st_size, 1, fp)) {
        fclose(fp);
        free(json_string);
        error_printf("deserialise_widgets_file: failed to read file data %s \n", filepath);
        return EXIT_FAILURE;
    }

    fclose(fp);
    int rv = deserialise_json(json_string, filestatus.st_size, ctx);
    free(json_string);

    if (rv != 0) {
        error_printf("deserialise_widgets_file: failed to parse json file %s\n", filepath);
    }
    return 0;
}
