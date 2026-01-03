/*
** Copyright 2007-2008 Logitech. All Rights Reserved.
** Copyright 2025 Blaise Dias
**
** This file is licensed under BSD. Please see the LICENSE file for details.
**
*/

#ifndef JIVE_TYPES_H
#define JIVE_TYPES_H
#include <SDL2/SDL_stdinc.h>

/* boolean type */
//typedef unsigned int bool;
//#define true 1
//#define false !true
#ifndef false
#define false 0
#define true !false
#endif 

/* ip3k type definitions */
#define bool SDL_bool
#define bool_t bool
#define TRUE true
#define FALSE false
#define u8_t Uint8
#define s8_t Sint8
#define u16_t Uint16
#define s16_t Sint16
#define u32_t Uint32
#define s32_t Sint32
#define u64_t Uint64
#define s64_t Sint64

#define MAX(a,b) (((a)>=(b))?(a):(b))
#define MIN(a,b) (((a)<=(b))?(a):(b))

#endif // JIVE_TYPES_H
