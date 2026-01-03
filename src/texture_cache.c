/*
** Copyright 2025 Blaise Dias. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#include <stdlib.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>
//#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>
#include "city.h"
#include "texture_cache.h"
#include "types.h"
#include "logging.h"

typedef struct tcache_entry tcache_entry;

struct tcache_entry {
    tcache_entry*       lru_prev;
    tcache_entry*       lru_next;
    const char*         path;
    uint32_t            hashv;
    texture_id_t        id;
    const SDL_Texture*  texture;
    bool                ejected;
};

// LRU linked list variables
static tcache_entry ht[3]= {
    { .lru_prev=&ht[1], .lru_next=&ht[1], .path="", },
    { .lru_prev=&ht[0], .lru_next=&ht[0], .path="", },
    { .path="", },
};
// LRU Head
static tcache_entry* lru_head = &ht[0];
// LRU Tail
static tcache_entry* lru_tail = &ht[1];
// Place holder
static tcache_entry* tce_empty = &ht[2];

#define PRIME2K 2039
#define PRIME4k 4093
#define PRIME8k 8191
#define PRIME12k 12281
#define PRIME16k 16381
#define PRIME32k 32749
#define PRIME48k 49057
#define PRIME64k 65521
#define PRIME128k 131071

#define HASHTPRIME PRIME4k
#define COLLISION_STEP PRIME32k
static tcache_entry* tbl[HASHTPRIME];

static uint32_t hashfn(const char* token) {
    return CityHash32(token, strlen(token));
}

// Add cache entry to the LRU list
static void recently_used(tcache_entry* tce) {
    if (tce) {
        if (tce->lru_prev) {
            // delink
            tce->lru_prev->lru_next = tce->lru_next;
            tce->lru_next->lru_prev = tce->lru_prev;
        } else {
            tcache_printf("recently_used: new: tce=%p %d %s\n", tce, tce->id, tce->path);
        }

        // insert at head
        tce->lru_next = lru_head->lru_next;
        tce->lru_next->lru_prev = tce;
        lru_head->lru_next = tce;
        tce->lru_prev = lru_head;
        tcache_printf("recently_used: tce=%p %d\n", tce, tce->id);
    } else {
        error_printf("recently_used: tce=%p\n", tce);
    }
}

// Add texture 
// path: path to image file - or unique string identifier
// texture: texture 
// ejectable:  whether the texture is ejectable
//             typically false for texture which cannot be loaded from image files.
//             for example textures created to render text.
// returns: texture ID (quick access slot in the hash table)
texture_id_t tcache_put_texture(const char* path, const SDL_Texture* texture, bool ejectable) {
    uint32_t hashv = hashfn(path);
    texture_id_t indx = hashv%HASHTPRIME;
    int hop_count = 0;

    for(int count=0; count < HASHTPRIME; ++count) {
        tcache_entry* tce = tbl[indx];
        if (tce == NULL || tce == tce_empty || (tce->hashv == hashv && 0 == strcmp(path,  tce->path))) {
            if (tce == NULL || tce == tce_empty) {
                tce = calloc(1, sizeof(*tce));
                if (tce == NULL) {
                    exit(EXIT_FAILURE);
                }
                tbl[indx] = tce;
                tce->path = strdup(path);
                tce->texture = texture;
                tce->hashv = hashv;
                tce->id = indx;
                tcache_printf("tcache_put_texture: new: tce=%p %d %s\n", tce, tce->id, tce->path);
            } else {
                if (texture && tce->texture) {
                    SDL_DestroyTexture((SDL_Texture*)tce->texture);
                    tce->texture = texture;
                }
                tce->ejected = false;
                tcache_printf("tcache_put_texture: changed: tce=%p %d %s\n", tce, tce->id, tce->path);
            }
            if (ejectable) {
                recently_used(tce);
            } else {
                if (tce->lru_prev) {
                    // delink
                    tce->lru_prev->lru_next = tce->lru_next;
                    tce->lru_next->lru_prev = tce->lru_prev;
                }
                tce->lru_prev = NULL;
                tce->lru_next = NULL;
            }
            return indx;
        }
        tcache_printf("tcache_put_texture: collision: hop:%d %d %u %s %s\n",
               hop_count, indx, hashv, path, tbl[indx]->path);
        indx = (indx+COLLISION_STEP)%HASHTPRIME;
        ++hop_count;
    }
    
    return -1;
}

// Set texture using texture ID
// texture ID: texture slot
// texture: texture 
// returns: true for success, false otherwise
//          false ==> invalid ID or uninitialised quick access slot.
bool tcache_set_texture(texture_id_t texture_id, SDL_Texture* texture) {
    if (texture_id < 0 || texture_id >= HASHTPRIME) {
        error_printf("tcache_set_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        if (tce->texture) {
            SDL_DestroyTexture((SDL_Texture*)tce->texture);
        }
        tce->texture = texture;
        tce->ejected = false;
        tcache_printf("tcache_set_texture: changed: tce=%p %d %s\n", tce, tce->id, tce->path);
        return true;
    } else {
        error_printf("tcache_set_texture: null entry: %d\n", texture_id);
    }
    return false;
}

// Get texture using the path/token
// path: path to image file - or unique string identifier
// texture_id*: quick access texture ID
// returns: texture, NULL is the texture is not found
//          texture ID or -1 is texture is not found
const SDL_Texture* tcache_get_texture(const char* path, texture_id_t* texture_id) {
    uint32_t hashv = hashfn(path);
    texture_id_t indx = hashv%HASHTPRIME;
    tcache_entry* tce = tbl[indx];

    for(int count=0; count < HASHTPRIME; ++count) {
        if (tce && (tce->hashv == hashv && 0 == strcmp(path,  tce->path))) {
//            tcache_printf("tcache_get_texture: OK: %s\n", tce->path);
            if (tce->lru_prev) {
                recently_used(tce);
            }
            if (texture_id) {
                *texture_id = indx;
            }
            return tce->texture;
        }
        indx = (indx+COLLISION_STEP)%HASHTPRIME;
        tce = tbl[indx];
//        tcache_printf("tcache_get_texture: not found: %u %s\n", hashv, path);
    }
    tcache_printf("tcache_get_texture: none: %u %s\n", hashv, path);
    if (texture_id) {
        *texture_id = -1;
    }
    return NULL;
}

// Get texture using the quick access texture ID
// texture_id*: quick access texture ID
// returns: texture, NULL is the texture is not found
//          texture ID or -1 is texture is not found
const SDL_Texture* tcache_quick_get_texture(texture_id_t texture_id) {
    if (texture_id < 0 || texture_id >= HASHTPRIME) {
        error_printf("tcache_quick_get_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce && tce != tce_empty) {
//        tcache_printf("tcache_quick_get_texture: %d %u %s\n", texture_id, tce->hashv, tce->path);
        if (tce->lru_prev) {
            recently_used(tce);
        }
        return tce->texture;
    } else {
    }
    error_printf("tcache_quick_get_texture: none: %d\n", texture_id);
    return NULL;
}

// Get texture ejected staaus
// texture_id*: quick access texture ID
// returns: texture, NULL is the texture is not found
//          texture ID or -1 is texture is not found
bool tcache_quick_get_texture_ejected(texture_id_t texture_id) {
    if (texture_id < 0 || texture_id >= HASHTPRIME) {
        error_printf("tcache_quick_get_texture_ejected: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        return tce->ejected;
    }
    error_printf("tcache_quick_get_texture_ejected: none: %d\n", texture_id);
    return false;
}

// Delete texture 
// texture_id*: quick access texture ID
bool tcache_quick_delete_texture(texture_id_t texture_id) {
    if (texture_id < 0 || texture_id >= HASHTPRIME) {
        error_printf("tcache_quick_delete_texture: invalid id: %d\n", texture_id);
        exit(EXIT_FAILURE);
        return false;
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        if (tce->lru_prev) {
            tcache_printf("tcache_quick_delete_texture: LRU delink: %d\n", texture_id);
            // delink
            tce->lru_prev->lru_next = tce->lru_next;
            tce->lru_next->lru_prev = tce->lru_prev;
        }
        if (tce->texture) {
            SDL_DestroyTexture((SDL_Texture* )tce->texture);
        }
        free((void *)tce->path);
        free(tce);
        tbl[texture_id] = tce_empty;
        return true;
    } else {
        error_printf("tcache_quick_delete_texture: none: %d\n", texture_id);
    }
    return false;
}

// Delete texture 
// path: path to image file - or unique string identifier
bool tcache_delete_texture(const char* path) {
    uint32_t hashv = hashfn(path);
    texture_id_t indx = hashv%HASHTPRIME;
    tcache_entry* tce = tbl[indx];

    tcache_printf("tcache_delete_texture: %s\n", path);
    for(int count=0; count < HASHTPRIME; ++count) {
        if (tce && (tce->hashv == hashv && 0 == strcmp(path,  tce->path))) {
            return tcache_quick_delete_texture(indx);
        }
        indx = (indx+7)%HASHTPRIME;
        tce = tbl[indx];
    }
    return false;

}

// Eject least recently used texture
bool tcache_eject_lru() {
    if (lru_tail->lru_prev != lru_head) {
        // delink
        tcache_entry* tce = lru_tail->lru_prev;
        tce->lru_prev->lru_next = lru_tail;
        lru_tail->lru_prev = tce->lru_prev;
        // lru_prev != NULL => ejectable
        tce->lru_prev = tce_empty;
        tce->ejected = true;
        if (tce->texture) {
            SDL_DestroyTexture((SDL_Texture* )tce->texture);
            tce->texture = NULL;
        }
        tcache_printf("tcache_eject_lru: %d %s\n", tce->id, tce->path);
        return true;
    } else {
        tcache_printf("tcache_eject_lru: nothing to eject\n");
    }
    return false;
}

// Load texture from file - 
// texture_id* : quick access texture ID
// renderer : SDL renderer context
// returns : texture, NULL is the texture is not found
//          texture ID or -1 is texture is not found
bool tcache_load_from_file(texture_id_t texture_id, SDL_Renderer* renderer) {
    if (texture_id < 0 || texture_id >= HASHTPRIME) {
        error_printf("tcache_load_from_file: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        if (tce->texture == NULL) {
            tce->texture = IMG_LoadTexture(renderer, tce->path);
            if (NULL == tce->texture) {
                error_printf("tcache_load_from_file: failed: %s\n", tce->path);
            }
            if (tce->lru_prev) {
                recently_used(tce);
            }
        }
        return tce->texture != NULL;
    }
    error_printf("tcache_load_from_file: none: %d\n", texture_id);
    return false;
}

// Load texture from file and add it to the texture cache.
// path : path to image file
// renderer : SDL renderer context
// ejectable : flag to enable ejecting of the texture
// returns: texture, NULL is the texture is not found
//          texture ID or -1 is texture is not found
texture_id_t  tcache_load_media(const char* path, SDL_Renderer* renderer, bool ejectable) {
    texture_id_t texture_id = tcache_put_texture(path, NULL, ejectable);
    tcache_printf("tcache_load_media: id=%d path=%s\n", texture_id, path);
    bool loaded = tcache_load_from_file(texture_id, renderer);
    if (!loaded) {
        tcache_eject_lru();
        loaded = tcache_load_from_file(texture_id, renderer);
    }
    tcache_printf("tcache_load_media: id=%d path=%s loaded=%u\n", texture_id, path, (unsigned)loaded);
    return texture_id;
}

void tcache_dump() {
    {
        int count = 0;
        int last_ix = 0;
        printf("texture cache dump:\n");
        printf("-----------------------------\n");
        for(int ix=0; ix < HASHTPRIME; ++ix) {
            tcache_entry* tce = tbl[ix];
            if (tce) {
                printf("    %d) delta=%4d hashv=%08x %p %p %p %s\n", ix, ix - last_ix,
                       tce->hashv,
                       tce->lru_prev,
                       tce,
                       tce->lru_next,
                       tce->path);
                ++count;
                last_ix = ix;
            }
        }
        printf("Occupancy %f %d/%d\n", ((float)count/HASHTPRIME)*100, count, HASHTPRIME);
    }
    printf("-----------------------------\n");
    printf("LRU:\n");
    {
        int lru_count = 0;
        for(tcache_entry* tce = lru_tail->lru_prev; tce != lru_head; tce = tce->lru_prev) {
                printf("    %d) hashv=%08x %p %p %p %s\n", tce->id,
                       tce->hashv,
                       tce->lru_prev,
                       tce,
                       tce->lru_next,
                       tce->path);
                ++lru_count;
        }
        printf("Occupancy %f %d/%d\n", ((float)lru_count/HASHTPRIME)*100, lru_count, HASHTPRIME);
    }
    printf("-----------------------------\n");
}
