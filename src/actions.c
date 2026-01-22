#include "application.h"
#include "actions.h"
#include "widgets.h"
#include "logging.h"

static SDL_Event quit_event = {.type = SDL_QUIT };
static SDL_Event next_visu_event = {.type = USEREVENT_NEXT_VISU };
static SDL_Event prev_visu_event = {.type = USEREVENT_PREV_VISU };
static SDL_Event next_vu_event = {.type = USEREVENT_NEXT_VU };
static SDL_Event prev_vu_event = {.type = USEREVENT_PREV_VISU };
static SDL_Event next_sp_event = {.type = USEREVENT_NEXT_SP };
static SDL_Event prev_sp_event = {.type = USEREVENT_PREV_SP };

static void action_quit(widget* wdgt) {
    SDL_PushEvent(&quit_event);
}

static void action_next_visu(widget* wdgt) {
    SDL_PushEvent(&next_visu_event);
}

static void action_prev_visu(widget* wdgt) {
    SDL_PushEvent(&prev_visu_event);
}

static void action_next_vu(widget* wdgt) {
    SDL_PushEvent(&next_vu_event);
}

static void action_prev_vu(widget* wdgt) {
    SDL_PushEvent(&prev_vu_event);
}

static void action_next_sp(widget* wdgt) {
    SDL_PushEvent(&next_sp_event);
}

static void action_prev_sp(widget* wdgt) {
    SDL_PushEvent(&prev_sp_event);
}

static void action_none(widget* wdgt) {
}

static void action_lock_vumeter(widget* wdgt) {
    widget *w = &wdgt->view->list->head;
    while(w != NULL) {
        if (w->type == WIDGET_VUMETER) {
            widget_vumeter_select_lock(w, true);
        }
        w = w->next;
    }
}

static void action_unlock_vumeter(widget* wdgt) {
    widget *w = &wdgt->view->list->head;
    while(w != NULL) {
        if (w->type == WIDGET_VUMETER) {
            widget_vumeter_select_lock(w, false);
        }
        w = w->next;
    }
}

static void action_lock_visu(widget* wdgt) {
    action_lock_vumeter(wdgt);
}

static void action_unlock_visu(widget* wdgt) {
    action_unlock_vumeter(wdgt);
}

static void widget_dispatch_action_explicit(widget* wdgt, action act) {
    action_printf("%p %d %s\n", wdgt, act, action_to_string(act));
    switch(act) {
        case ACTION_NONE:
            action_none(wdgt);
            break;
        case ACTION_QUIT:
            action_quit(wdgt);
            break;
        case ACTION_NEXT_VISU:
            action_next_visu(wdgt);
            break;
        case ACTION_PREV_VISU:
            action_prev_visu(wdgt);
            break;
        case ACTION_NEXT_VU:
            action_next_vu(wdgt);
            break;
        case ACTION_PREV_VU:
            action_prev_vu(wdgt);
            break;
        case ACTION_NEXT_SP:
            action_next_sp(wdgt);
            break;
        case ACTION_PREV_SP:
            action_prev_sp(wdgt);
            break;
        case ACTION_LOCK_VUMETER:
            action_lock_vumeter(wdgt);
            break;
        case ACTION_UNLOCK_VUMETER:
            action_unlock_vumeter(wdgt);
            break;
        case ACTION_LOCK_VISU:
            action_lock_visu(wdgt);
            break;
        case ACTION_UNLOCK_VISU:
            action_unlock_visu(wdgt);
            break;
        case ACTION_MULTISTATE_BUTTON:
            error_printf("multistate action %d for %p type=%d\n", act, wdgt, wdgt->type);
            break;

        case ACTION_PLAY:
            break;
        case ACTION_PAUSE:
            break;
        case ACTION_STOP:
            break;

        case ACTION_NEXT_TRACK:
            break;
        case ACTION_PREV_TRACK:
            break;

        case ACTION_REPEAT_ONCE:
            break;
        case ACTION_REPEAT:
            break;
        case ACTION_REPEAT_OFF:
            break;

        case ACTION_SHUFFLE:
            break;
        case ACTION_SHUFFE_ALBUM:
            break;
        case ACTION_SHUFFLE_OFF:
            break;

        case ACTION_MUSIC_INFORMATION:
            break;

        case ACTION_SET_VOLUME:
            if (wdgt->type == WIDGET_SLIDER) {
                int level;
                widget_slider_get_value(wdgt, &level);
                action_printf("volume level = %d\n", level);
            }
            break;

        case ACTION_END:
            break;
        default:
            error_printf("unknown action %d for %p type=%d\n", act, wdgt, wdgt->type);
            break;
    }
}

static void action_multi_state_button(widget* wdgt) {
    action_printf("action_multistate_button action state=%d %d %s\n", 
            wdgt->sub.multistate_button.state,
            wdgt->sub.multistate_button.res[wdgt->sub.multistate_button.state].action,
            action_to_string( wdgt->sub.multistate_button.res[wdgt->sub.multistate_button.state].action));
    widget_dispatch_action_explicit(wdgt, wdgt->sub.multistate_button.res[wdgt->sub.multistate_button.state].action);
    wdgt->sub.multistate_button.state = (wdgt->sub.multistate_button.state + 1) % wdgt->sub.multistate_button.state_count;
}

void widget_dispatch_action(widget* wdgt) {
    switch(wdgt->action) {
        default:
            widget_dispatch_action_explicit(wdgt, wdgt->action);
            break;
        case ACTION_MULTISTATE_BUTTON:
            action_multi_state_button(wdgt);
            break;
    }
}

static const char* action_strings[] = {
        "",                 /* NONE */
        "quit",
        "next_visu",
        "prev_visu",
        "next_vu",
        "prev_vu",
        "next_sp",
        "prev_sp",
        "lock_vumeter",
        "unlock_vumeter",
        "lock_visu",
        "unlock_visu",
        "multistate_button",

        "play",
        "pause",
        "stop",

        "next-track",
        "previous-track",

        "repeat-once",
        "repeat",
        "repeat-off",

        "shuffle",
        "shuffle-album",
        "shuffle-off",

        "music-information",

        "set-volume-level",

        "",                 /* END */
};

action action_from_string(const char* str) {
    if (str != NULL ) {
        for(int a=0; a < sizeof(action_strings)/sizeof(action_strings[0]); a++) {
            if (0 == strcmp(action_strings[a], str)) {
                return a;
            }
        }
    }
    if (str) {
        error_printf("unknown action %s\n", str);
    }
    return ACTION_NONE;
}

const char* action_to_string(action action) {
    if (action > ACTION_NONE && action < ACTION_END ) {
        return action_strings[action];
    }
    if (action == ACTION_NONE) return "ACTION_NONE";
    if (action == ACTION_END) return "ACTION_NONE(END)";
    return "ACTION_UNKNOWN";
}
