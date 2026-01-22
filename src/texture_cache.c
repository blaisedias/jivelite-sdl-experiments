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
#include "timing.h"

typedef struct tcache_entry tcache_entry;

struct tcache_entry {
    // value of 0 => inuse forever
    int64_t             inuse_counter;
    const char*         path;
    uint32_t            hashv;
    const SDL_Texture*  texture;
    SDL_Surface*        surface;
    int                 w,h;
    int                 num_bytes;
    bool                ejected;
    bool                locked;
};

static tcache_entry empty_tce = {
    .path = "___empty___",
};
static void tcache_cap_num_bytes(unsigned inc);

int64_t global_inuse_counter = 1;
unsigned num_texture_bytes = 0;
unsigned max_num_texture_bytes = 0;
unsigned resolution_req_counter;
unsigned resolution_done_counter;

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
#define NUM_TBL_ENTRIES HASHTPRIME+1
static tcache_entry* tbl[NUM_TBL_ENTRIES];

static SDL_threadID renderer_tid;

void tcache_set_renderer_tid(const SDL_threadID tid) {
    if (renderer_tid == 0) {
        renderer_tid = tid; 
    } else {
        printf("Changing renderer thread ID is unsupported\n");
        exit(EXIT_FAILURE);
    }
}

//
bool check_permitted() {
    return SDL_GetThreadID(NULL) == renderer_tid;
}

// only for used by quick sort
static void swap_tcache_entries(tcache_entry** a, tcache_entry** b) {
  tcache_entry* t = *a;
  *a = *b;
  *b = t;
}

static int qs_tcache_partition(tcache_entry** tce_arr, int lo, int hi) {
    // last element is the pivot
    tcache_entry* pivot = tce_arr[hi];

    // temp pivot
    int i = lo-1;
    for (int j = lo; j < hi; j++) {
        // if the current element <= pivot 
        if (tce_arr[j]->inuse_counter <= pivot->inuse_counter) {
            // move temporary pivot index forward
            ++i;
            // swap current element with temporary pivot
            swap_tcache_entries(tce_arr+i, tce_arr+j);
        }
    }
    // swap last element with pivot
    swap_tcache_entries(tce_arr+i+1, tce_arr+hi);
    // return pivot index
    return i+1;
}

static void qs_tcache_range(tcache_entry** tce_arr, int lo, int hi) {
    if (lo < hi) {
        // partition the array to get the pivot index
        int pivot = qs_tcache_partition(tce_arr, lo, hi);
        // sort the left side
        qs_tcache_range(tce_arr, lo, pivot-1);
        // sort the right side
        qs_tcache_range(tce_arr, pivot+1, hi);
    }
}

static void quick_sort_tcache(tcache_entry** tce_arr, int num_elements) {
    qs_tcache_range(tce_arr, 0, num_elements-1);
}


void tcache_init(void) {
    static bool initialised = false;
    if (!initialised) {
        initialised = true;
        tbl[NUM_TBL_ENTRIES-1] = &empty_tce;
    }
}

static uint32_t hashfn(const char* token) {
    return CityHash32(token, strlen(token));
}

static void recently_used(tcache_entry* tce) {
    if (tce) {
        tce->inuse_counter = ++global_inuse_counter;
//        tcache_printf("recently_used: tce=%p %ld %s\n", tce, tce->inuse_counter, tce->path);
    } else {
        error_printf("recently_used: tce=%p\n", tce);
    }
}

static void release_texture(tcache_entry* tce) {
    if (tce && tce->texture) {
        SDL_DestroyTexture((SDL_Texture*)tce->texture);
        tce->texture = NULL;
        num_texture_bytes -= tce->num_bytes;
        tce->w = tce->h = tce->num_bytes = 0;
        tcache_printf("release_texture: texture_bytes=%d\n", num_texture_bytes);
    }
}

static void update_texture(tcache_entry* tce, const SDL_Texture* texture) {
    if (tce) {
        if (tce->texture) {
            release_texture(tce);
        }
        if (texture) {
            Uint32 fmt;
            if (0 == SDL_QueryTexture((SDL_Texture*)texture, &fmt, NULL, &tce->w, &tce->h)) {
                tce->num_bytes = SDL_BYTESPERPIXEL(fmt) * tce->w * tce->h;
                num_texture_bytes += tce->num_bytes;
                tcache_printf("update_texture: texture_bytes=%d\n", num_texture_bytes);
            }
            tce->texture = texture;
        }
        // TODO: do this before creating the texture
        tcache_cap_num_bytes(0);
    }
}


// Add texture 
// path: path to image file - or unique string identifier
// texture: texture 
//             typically false for texture which cannot be loaded from image files.
//             for example textures created to render text.
// returns: texture ID (quick access slot in the hash table)
texture_id_t tcache_put_texture(const char* path, const SDL_Texture* texture) {
    if (!check_permitted()) {
        return -1;
    }

    uint32_t hashv = hashfn(path);
    texture_id_t indx = hashv%HASHTPRIME;
    int hop_count = 0;

    tcache_init();
    for(int count=0; count < HASHTPRIME; ++count) {
        tcache_entry* tce = tbl[indx];
        // indx 0 -> reserved for uninitialised 
        if (indx) {
            if (tce == NULL || (tce->hashv == hashv && 0 == strcmp(path,  tce->path))) {
                if (tce == NULL) {
                    tce = calloc(1, sizeof(*tce));
                    if (tce == NULL) {
                        error_printf("tcache_put_texture: Out of memory\n");
                        exit(EXIT_FAILURE);
                    }
                    tbl[indx] = tce;
                    tce->path = strdup(path);
                    update_texture(tce, texture);
                    tce->hashv = hashv;
                    tcache_printf("tcache_put_texture: new: tce=%p %d %s\n", tce, indx, tce->path);
                } else {
                    update_texture(tce, texture);
                    tce->ejected = false;
                    tcache_printf("tcache_put_texture: changed: tce=%p %d %s\n", tce, indx, tce->path);
                }
                recently_used(tce);
                return indx;
            }
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
bool tcache_set_texture(texture_id_t texture_id, const SDL_Texture* texture) {
    if (!check_permitted()) {
        return false;
    }
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_set_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        update_texture(tce, texture);
        tce->ejected = false;
        tcache_printf("tcache_set_texture: changed: tce=%p %d %s\n", tce, texture_id, tce->path);
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
//          texture ID or INVALID_TEXTURE_ID is texture is not found
SDL_Texture* tcache_get_texture(const char* path, texture_id_t* texture_id) {
    if (!check_permitted()) {
        return NULL;
    }
    uint32_t hashv = hashfn(path);
    texture_id_t indx = hashv%HASHTPRIME;
    tcache_entry* tce = tbl[indx];

    for(int count=0; count < HASHTPRIME; ++count) {
        if (tce && (tce->hashv == hashv && 0 == strcmp(path,  tce->path))) {
//            tcache_printf("tcache_get_texture: OK: %s\n", tce->path);
            recently_used(tce);

            if (texture_id) {
                *texture_id = indx;
            }
            // stored as a const - callers require a pointer which is not const
            return (SDL_Texture *)tce->texture;
        }
        indx = (indx+COLLISION_STEP)%HASHTPRIME;
        tce = tbl[indx];
//        tcache_printf("tcache_get_texture: not found: %u %s\n", hashv, path);
    }
    tcache_printf("tcache_get_texture: none: %u %s\n", hashv, path);
    if (texture_id) {
        *texture_id = INVALID_TEXTURE_ID;
    }
    return NULL;
}

// Get texture using the quick access texture ID
// texture_id*: quick access texture ID
// returns: texture, NULL is the texture is not found
//          texture ID
SDL_Texture* tcache_quick_get_texture(texture_id_t texture_id) {
    if (!check_permitted()) {
        return NULL;
    }
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_quick_get_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
//        tcache_printf("tcache_quick_get_texture: %d %u %s\n", texture_id, tce->hashv, tce->path);
        recently_used(tce);

        // stored as a const - callers require a pointer which is not const
        return (SDL_Texture *)tce->texture;
    }
    error_printf("tcache_quick_get_texture: none: %d\n", texture_id);
    return NULL;
}

// Get texture ejected staaus
// texture_id*: quick access texture ID
// returns: texture, NULL is the texture is not found
//          texture ID or -1 is texture is not found
bool tcache_quick_get_texture_ejected(texture_id_t texture_id) {
    if (!check_permitted()) {
        return false;
    }
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
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
    if (!check_permitted()) {
        return false;
    }
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_quick_delete_texture: invalid id: %d\n", texture_id);
        exit(EXIT_FAILURE);
        return false;
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        tcache_printf("tcache_quick_delete_texture: LRU delink: %d %p\n", texture_id, tce);
        release_texture(tce);
        if (tce->path) {
            free((void *)tce->path);
        }
        free(tce);
        tbl[texture_id] = NULL;
        return true;
    } else {
        error_printf("tcache_quick_delete_texture: none: %d\n", texture_id);
    }
    return false;
}

// Delete texture 
// path: path to image file - or unique string identifier
bool tcache_delete_texture(const char* path) {
    if (!check_permitted()) {
        return false;
    }
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

static int lru_sort_tce(tcache_entry** lru_sorted_tbl) {
    int indx = 0;
    for(int ix=0; ix < HASHTPRIME; ++ix) {
        tcache_entry* tce = tbl[ix];
        if (tce) {
           lru_sorted_tbl[indx] = tce;
            ++indx;
        }
    }
    quick_sort_tcache(lru_sorted_tbl, indx);
    return indx;
}

static bool cap_exceeded(int increment, int ix) {
    return max_num_texture_bytes && (num_texture_bytes + increment) > max_num_texture_bytes;
}

// Eject least recently used textures to reduce texture bytes to the configured limit
static bool tcache_eject(unsigned increment, bool (*check)(int, int)) {
    static tcache_entry* eject_tbl[HASHTPRIME];
    bool ejected = false;
    int count = lru_sort_tce(eject_tbl);
    for(int ix=0; ix < count && check(increment, ix); ++ix) {
        tcache_entry* tce = eject_tbl[ix];
        if (!tce->locked && tce->texture) {
            release_texture(tce);
            tcache_eject_printf("tcache_cap_num_bytes: ejected %s %d %u / %u\n", tce->path, increment, num_texture_bytes, max_num_texture_bytes);
            ejected = true;
        }
    }
    return ejected;
}

// Eject least recently used textures to reduce texture bytes to the configured limit
static void tcache_cap_num_bytes(unsigned increment) {
    tcache_eject(increment, cap_exceeded);
}

static bool test_cap_exceeded(int increment, int ix) {
    return ix == 0;
}

// TESTING ONLY function: Eject least recently used texture: use for testing LRU 
bool tcache_test_lru_eject() {
    if (check_permitted()) {
        return tcache_eject(0, test_cap_exceeded);
    }
    return false;
}

// Load texture from file - 
// texture_id* : quick access texture ID
// renderer : SDL renderer context
// returns : texture, NULL is the texture is not found
//          texture ID or -1 is texture is not found
bool tcache_load_from_file(texture_id_t texture_id, SDL_Renderer* renderer) {
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_load_from_file: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce == &empty_tce) {
        return true;
    }
    if (tce) {
        // Only load if not previously loaded
        if (tce->texture == NULL) {
           if (tce->surface == NULL) {
                tcache_printf("tcache_load_from_file: : %d %s\n", texture_id, tce->path);
                tce->surface = IMG_Load(tce->path);
                if (tce->surface == NULL)  {
                    error_printf("tcache_load_from_file: failed: %d %s\n", texture_id, tce->path);
                } else {
                    tce->w = tce->surface->w;
                    tce->h = tce->surface->h;
                    // signal texture resultion required
                    ++resolution_req_counter;
                }
           }
        }
        recently_used(tce);
        return tce->texture != NULL || tce->surface !=NULL; 
    }
    error_printf("tcache_load_from_file: none: %d\n", texture_id);
    return false;
}

// Load texture from file and add it to the texture cache.
// path : path to image file
// renderer : SDL renderer context
// returns: texture ID
texture_id_t tcache_load_media(const char* path, SDL_Renderer* renderer, bool* ploaded) {
    texture_id_t texture_id = tcache_put_texture(path, NULL);
    tcache_printf("tcache_load_media: id=%d path=%s\n", texture_id, path);
    bool loaded = tcache_load_from_file(texture_id, renderer);
    if (ploaded) {
        *ploaded = loaded;
    }
    tcache_printf("tcache_load_media: id=%d path=%s loaded=%u\n", texture_id, path, (unsigned)loaded);
    return texture_id;
}

void tcache_dump() {
    static tcache_entry* stbl[HASHTPRIME];
    {
        int count = 0;
        int last_ix = 0;
        int ix_s = 0;
        printf("texture cache dump:\n");
        printf("-----------------------------\n");
        for(int ix=0; ix < HASHTPRIME; ++ix) {
            tcache_entry* tce = tbl[ix];
            if (tce) {
                printf("    %05d) delta=%4d hashv=%08x inuse=%016lx %s %p w=%4d h=%4d bytes=%8d %s\n",
                       ix, ix - last_ix,
                       tce->hashv,
                       tce->inuse_counter,
                       tce->locked ? "locked  ": "unlocked",
                       tce,
                       tce->w,
                       tce->h,
                       tce->num_bytes,
                       tce->path);
                ++count;
                last_ix = ix;
                stbl[ix_s] = tce;
                ++ix_s;
            }
        }
/*        
        printf("Number of hashtable entries=%d\n", HASHTPRIME);
        printf("Occupancy %f %d/%d\n", ((float)count/HASHTPRIME)*100, count, HASHTPRIME);
        printf("Memory used for table entries = %ld\n", count * sizeof(tcache_entry));
        printf("Sizeof cache_entry = %ld\n", sizeof(tcache_entry));
        printf("Sizeof table = %ld\n", sizeof(tbl));
*/
//        qsort(stbl, count, sizeof(stbl[0]), tcache_compare);
        quick_sort_tcache(stbl, count);
        printf("LRU: ------------------------\n");
        int64_t locked_texture_bytes=0;
        int64_t unlocked_texture_bytes=0;
        int64_t ejected_texture_bytes=0;
        for(int ix=0; ix < count; ++ix) {
            tcache_entry* tce = stbl[ix];
                printf("    %5d) hashv=%08x inuse=%016lx %p %s\n", ix,
                       tce->hashv,
                       tce->inuse_counter,
                       tce,
                       tce->path);
                if (tce->ejected) {
                    ejected_texture_bytes += tce->num_bytes;
                } else if (tce->locked) {
                    locked_texture_bytes += tce->num_bytes;
                } else {
                    unlocked_texture_bytes += tce->num_bytes;
                }
        }
        printf("Number of hashtable entries=%d\n", HASHTPRIME);
        printf("Occupancy %f %d/%d\n", ((float)count/HASHTPRIME)*100, count, HASHTPRIME);
        printf("Memory used for table entries = %ld\n", count * sizeof(tcache_entry));
        printf("Sizeof cache_entry = %ld\n", sizeof(tcache_entry));
        printf("Sizeof table = %ld\n", sizeof(tbl));
        printf("Texture bytes = %u %f MiB, locked=%ld %f MiB, unlocked=%ld %f MiB, ejected=%ld %f MiB\n", 
                num_texture_bytes, (float)num_texture_bytes/(1024*1024),
                locked_texture_bytes, (float)locked_texture_bytes/(1024*1024),
                unlocked_texture_bytes, (float)unlocked_texture_bytes/(1024*1024),
                ejected_texture_bytes, (float)ejected_texture_bytes/(1024*1024));
    }
    printf("-----------------------------\n");
}

unsigned tcache_get_texture_bytes_count(void) {
    return num_texture_bytes;
}

texture_id_t tcache_get_empty_tid(void) {
    tcache_init();
    return NUM_TBL_ENTRIES -1;
}

bool tcache_lock_texture(texture_id_t texture_id) {
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_lock_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        tce->locked = true;
    }
    return tce != NULL;
}

bool tcache_unlock_texture(texture_id_t texture_id) {
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_unlock_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        tce->locked = false;
    }
    return tce != NULL;
}

// Get the texture id matching a token
// This function can be expensive in terms of execution time,
// especially if the token does not exist
texture_id_t tcache_get_texture_id(const char* token) {
    uint32_t hashv = hashfn(token);
    texture_id_t indx = hashv%HASHTPRIME;

    for(int count=0; count < HASHTPRIME; ++count, ++indx) {
        tcache_entry* tce = tbl[indx];
        if (tce && (tce->hashv == hashv && 0 == strcmp(token,  tce->path))) {
            return indx;
        }
    }
    return INVALID_TEXTURE_ID;
}

void tcache_resolve_textures(SDL_Renderer* renderer) {
    tcache_init();
    if (!check_permitted()) {
        return;
    }
    // signalling between threads.
    // synchronisation primitives could be used but this is simpler and lockless.
    // When an image is loaded into a surface the request counter is incremented
    // When resolution is performed the done counter is synchronised with the req counter
    // since *all* resolutions are perfromed.
    if (resolution_req_counter == resolution_done_counter) {
        return;
    }
    // Resolution is required:
    // BEFORE resolving synchronise the request and donecounter values,
    // then if during resolution an image is loaded in another thread the counters will
    // not be equal the next time tcache_resolve_textures is invoked and resolution will occur.
    // An extra resolution scan may be performed, that is deemed an acceptable trade-off.
    uint64_t ms_0 = get_micro_seconds();
    resolution_done_counter = resolution_req_counter;
    for(texture_id_t ix=0; ix < HASHTPRIME; ++ix) {
        tcache_entry* tce = tbl[ix];
        if (tce && tce->texture==NULL && tce->surface !=NULL) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, tce->surface);
            if (NULL == texture) {
                error_printf("tcache_resolve_textures: failed: %s %s\n", ix, tce->path, SDL_GetError());
                SDL_ClearError();
            }
            update_texture(tce, texture);
            SDL_FreeSurface(tce->surface);
            tce->surface = NULL;
        }
    }
    uint64_t ms_1 = get_micro_seconds();
    perf_printf("\ntexture_resolve: %5.2f millis\n", (float)(ms_1 - ms_0)/1000);
}

// Get texture width and height 
// texture_id*: quick access texture ID
// returns: true if the texture dimensions could be determined
bool tcache_quick_get_texture_dimensions(texture_id_t texture_id, int* w, int* h) {
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_quick_get_texture_ejected: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce) {
        if (tce->texture || tce->surface) {
            *w = tce->w;
            *h = tce->h;
        }
        return true;
    }
    return false;
}

void tcache_set_limit(unsigned limit) {
    max_num_texture_bytes = limit;
}
