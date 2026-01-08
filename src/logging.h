/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/


#ifndef  _logging_h_
#define _logging_h_
extern void error_printf(char *format, ...);
extern void dummy_printf(char *format, ...);

extern void (*vol_printf)(char *format, ...);
extern void (*perf_printf)(char *format, ...);
extern void (*load_printf)(char *format, ...);
extern void (*scale_printf)(char *format, ...);
extern void (*input_printf)(char *format, ...);
extern void (*debug_printf)(char *format, ...);
extern void (*tcache_printf)(char *format, ...);
extern void (*frame_perf_printf)(char *format, ...);
extern void (*json_printf)(char *format, ...);
extern void (*action_printf)(char *format, ...);

typedef enum {
    DEBUG_PRINTF,
    VOL_PRINTF,
    PERF_PRINTF,
    LOAD_PRINTF,
    SCALE_PRINTF,
    INPUT_PRINTF,
    TEXTURE_CACHE_PRINTF,
    FRAME_PERF_PRINTF,
    JSON_PRINTF,
    ACTION_PRINTF,
}vu_printf_typ;

void enable_printf(vu_printf_typ v);
void disable_printf(vu_printf_typ v);

#endif
