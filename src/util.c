#include <SDL2/SDL.h>

static float screen_orientation=0.0f;
static int screen_width, screen_height;

void (*translate_xy)(int* x, int* y);
void (*translate_point)(SDL_Point* pt);
void (*translate_image_rect)(SDL_Rect* rect);
void (*translate_draw_rect)(SDL_Rect* rect);

void copyRect(const SDL_Rect *src, SDL_Rect *dst) {
    dst->x = src->x;
    dst->y = src->y;
    dst->w = src->w;
    dst->h = src->h;
}

static void xlate_point_0(SDL_Point* pt) {}

static void xlate_point_180(SDL_Point* pt) {
    pt->x = screen_width - pt->x;
    pt->y = screen_height - pt->y;
}

static void xlate_point_90(SDL_Point* pt) {
    int x =  pt->x;
    int y =  pt->y;
    pt->x = y;
    pt->y = screen_width - x;
}

static void xlate_point_270(SDL_Point* pt) {
    int x = pt->x;
    int y = pt->y;
    pt->x = screen_height - y;
    pt->y = x;
}

static void xlate_image_rect_0(SDL_Rect* rect) {}

static void xlate_image_rect_180(SDL_Rect* rect) {
    rect->x = screen_width - rect->x - rect->w;
    rect->y = screen_height - rect->y - rect->h;
}

static void xlate_image_rect_90(SDL_Rect* rect) {
    int x =  rect->x;
    int y =  rect->y;
    rect->x = screen_width - rect->h - y + ((rect->h - rect->w)/2);
    rect->y = x + ((rect->w - rect->h)/2);
}

static void xlate_image_rect_270(SDL_Rect* rect) {
    int x =  rect->x;
    int y =  rect->y;
    rect->x = y + ((rect->h - rect->w)/2);
    rect->y = screen_height - rect->w -x + ((rect->w - rect->h)/2);
}

static void xlate_draw_rect_0(SDL_Rect* rect) {}

static void xlate_draw_rect_180(SDL_Rect* rect) {
    rect->x = screen_width - rect->x - rect->w;
    rect->y = screen_height - rect->y - rect->h;
}

static void xlate_draw_rect_90(SDL_Rect* rect) {
    int x =  rect->x;
    int y =  rect->y;
    int w =  rect->w;
    int h =  rect->h;
    rect->x = screen_width - h - y;
    rect->y = x;
    rect->w = h;
    rect->h = w;
}

static void xlate_draw_rect_270(SDL_Rect* rect) {
    int x =  rect->x;
    int y =  rect->y;
    int w =  rect->w;
    int h =  rect->h;
    rect->x = y;
    rect->y = screen_height - x - w;
    rect->w = h;
    rect->h = w;
}


static void xlate_xy_0(int* px, int *py) {}
static void xlate_xy_180(int* px, int *py) {
        *px = screen_width - (*px);
        *py = screen_height - (*py);
}
static void xlate_xy_90(int* px, int *py) {
        int x =  *px;
        int y =  *py;
        *px = y;
        *py = screen_width - x;
}
static void xlate_xy_270(int* px, int *py) {
        int x = *px;
        int y =  *py;
        *px = screen_height - y;
        *py = x;
}

void setup_orientation(float orientation, int w, int h, SDL_Rect *screen) {
    screen_orientation = orientation;
    screen_width = w;
    screen_height = h;
    screen->w = w;
    screen->h = h;

    translate_xy = xlate_xy_0;
    translate_point = xlate_point_0;
    translate_image_rect = xlate_image_rect_0;
    translate_draw_rect = xlate_draw_rect_0;

    if (orientation == 90) {
        translate_xy = xlate_xy_90;
        translate_point = xlate_point_90;
        translate_image_rect = xlate_image_rect_90;
        translate_draw_rect = xlate_draw_rect_90;
        screen->w = h;
        screen->h = w;
    }

    if (orientation == 180) {
        translate_xy = xlate_xy_180;
        translate_point = xlate_point_180;
        translate_image_rect = xlate_image_rect_180;
        translate_draw_rect = xlate_draw_rect_180;
    }

    if (orientation == 270) {
        translate_xy = xlate_xy_270;
        translate_point = xlate_point_270;
        translate_image_rect = xlate_image_rect_270;
        translate_draw_rect = xlate_draw_rect_270;
        screen->w = h;
        screen->h = w;
    }
}
