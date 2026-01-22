#ifndef __jl_action_h_
#define __jl_action_h_
/*
void action_quit(widget* wdgt);
void action_next_visu(widget* wdgt);
void action_prev_visu(widget* wdgt);
void action_next_vu(widget* wdgt);
void action_prev_vu(widget* wdgt);
void action_next_sp(widget* wdgt);
void action_prev_sp(widget* wdgt);
void action_none(widget* wdgt);

void action_lock_vumeter(widget* wdgt);
void action_unlock_vumeter(widget* wdgt);

void action_lock_visu(widget* wdgt);
void action_unlock_visu(widget* wdgt);
*/

typedef enum {
    ACTION_NONE,
    ACTION_QUIT,
    ACTION_NEXT_VISU,
    ACTION_PREV_VISU,
    ACTION_NEXT_VU,
    ACTION_PREV_VU,
    ACTION_NEXT_SP,
    ACTION_PREV_SP,
    ACTION_LOCK_VUMETER,
    ACTION_UNLOCK_VUMETER,
    ACTION_LOCK_VISU,
    ACTION_UNLOCK_VISU,
    ACTION_MULTISTATE_BUTTON,

    ACTION_PLAY,
    ACTION_PAUSE,
    ACTION_STOP,

    ACTION_NEXT_TRACK,
    ACTION_PREV_TRACK,

    ACTION_REPEAT_ONCE,
    ACTION_REPEAT,
    ACTION_REPEAT_OFF,

    ACTION_SHUFFLE,
    ACTION_SHUFFE_ALBUM,
    ACTION_SHUFFLE_OFF,

    ACTION_MUSIC_INFORMATION,

    ACTION_SET_VOLUME,

    ACTION_END,
} action;

action action_from_string(const char* str);
const char* action_to_string(action action); 
#endif // __jl_action_h_
