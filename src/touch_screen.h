#ifndef __touch_screen_h_
#define __touch_screen_h_

#include <tslib.h>

typedef struct {
    void (*up)(int x , int y);
    void (*down)(int x , int y);
    void (*motion)(int x , int y);
}touch_screen_svc_config;

int touch_screen_service(touch_screen_svc_config* config);
void stop_touch_screen_service(void);
#endif
