/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "logging.h"

static void logfprintf(char *format, ...) {
	struct timeval t;
	struct tm tm;
	gettimeofday(&t, NULL);
	gmtime_r(&t.tv_sec, &tm);
    fprintf(stdout, "%04d%02d%02d %02d:%02d:%02d.%03ld ",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				(long)(t.tv_usec / 1000));
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
	fflush(stdout);
}

void error_printf(char *format, ...) {
	struct timeval t;
	struct tm tm;
	gettimeofday(&t, NULL);
	gmtime_r(&t.tv_sec, &tm);
    fprintf(stderr, "%04d%02d%02d %02d:%02d:%02d.%03ld ",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				(long)(t.tv_usec / 1000));
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
void (*tcache_printf)(char *format, ...) = dummy_printf;
void (*tcache_eject_printf)(char *format, ...) = dummy_printf;
void (*profile_printf)(char *format, ...) = dummy_printf;
void (*profile_texture_printf)(char *format, ...) = dummy_printf;
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
        case TEXTURE_CACHE_EJECT_PRINTF:
            tcache_eject_printf = logfprintf;
            break;
        case PROFILE_PERF_PRINTF:
            profile_printf = logfprintf;
            break;
        case PROFILE_TEXTURE_PERF_PRINTF:
            profile_texture_printf = logfprintf;
            break;
        case JSON_PRINTF:
            json_printf = logfprintf;
            break;
        case ACTION_PRINTF:
            action_printf = logfprintf;
            break;
    }
}

void disable_printf(vu_printf_typ v) {
    switch(v) {
        case DEBUG_PRINTF:
            debug_printf = dummy_printf;
            break;
        case VOL_PRINTF:
            vol_printf = dummy_printf;
            break;
        case PERF_PRINTF:
            perf_printf = dummy_printf;
            break;
        case LOAD_PRINTF:
            load_printf = dummy_printf;
            break;
        case SCALE_PRINTF:
            scale_printf = dummy_printf;
            break;
        case INPUT_PRINTF:
            input_printf = dummy_printf;
            break;
        case TEXTURE_CACHE_PRINTF:
            tcache_printf = dummy_printf;
            break;
        case TEXTURE_CACHE_EJECT_PRINTF:
            tcache_eject_printf = dummy_printf;
            break;
        case PROFILE_PERF_PRINTF:
            profile_printf = dummy_printf;
            break;
        case PROFILE_TEXTURE_PERF_PRINTF:
            profile_texture_printf = dummy_printf;
            break;
        case JSON_PRINTF:
            json_printf = dummy_printf;
            break;
        case ACTION_PRINTF:
            action_printf = dummy_printf;
            break;
    }
}

