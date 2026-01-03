/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#include <stdio.h>
#include <stdarg.h>
#include "logging.h"

static void logfprintf(char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
	fflush(stdout);
}

void error_printf(char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fflush(stderr);
}

void dummy_printf(char *format, ...) {
    va_list args;
    va_start(args, format);
    va_end(args);
}

void (*vol_printf)(char *format, ...) = dummy_printf;
void (*perf_printf)(char *format, ...) = dummy_printf;
void (*load_printf)(char *format, ...) = dummy_printf;
void (*scale_printf)(char *format, ...) = dummy_printf;
void (*input_printf)(char *format, ...) = dummy_printf;
void (*debug_printf)(char *format, ...) = dummy_printf;
void (*tcache_printf)(char *format, ...) = logfprintf;
void (*frame_perf_printf)(char *format, ...) = dummy_printf;
void (*json_printf)(char *format, ...) = dummy_printf;
void (*action_printf)(char *format, ...) = dummy_printf;


void enable_printf(vu_printf_typ v) {
    switch(v) {
        case DEBUG_PRINTF:
            debug_printf = logfprintf;
            break;
        case VOL_PRINTF:
            vol_printf = error_printf;
            break;
        case PERF_PRINTF:
            perf_printf = error_printf;
            break;
        case LOAD_PRINTF:
            load_printf = logfprintf;
            break;
        case SCALE_PRINTF:
            scale_printf = logfprintf;
            break;
        case INPUT_PRINTF:
            input_printf = logfprintf;
            break;
        case TEXTURE_CACHE_PRINTF:
            tcache_printf = logfprintf;
            break;
        case FRAME_PERF_PRINTF:
            frame_perf_printf = logfprintf;
            break;
        case JSON_PRINTF:
            json_printf = logfprintf;
            break;
        case ACTION_PRINTF:
            action_printf = logfprintf;
            break;
    }
}

