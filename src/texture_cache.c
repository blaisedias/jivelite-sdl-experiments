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
#include <assert.h>

typedef struct tcache_entry tcache_entry;

struct tcache_entry {
    uint32_t            lru_count;
    const char*         path;
    uint32_t            hashv;
    const SDL_Texture*  texture;
    SDL_Surface*        surface;
    int                 w,h;
    int                 num_bytes;
    bool                ejected;
    bool                locked;
    bool                delete;
};

static tcache_entry empty_tce = {
    .path = "___empty___",
};
// When a table entry is deleted replacing it with NULL would
// break searching for the entry by string path, because NULL 
// indicates further probing is not required.
// On delete the entries are set to point to the deleted entry
// with NULL as the string pointer, so comparing strings should always fail.
static tcache_entry deleted_entry = {
};
static tcache_entry* tce_deleted=&deleted_entry;

static void tcache_cap_num_bytes(unsigned inc);

static inline bool external_tce(tcache_entry* tce) {
    return tce != NULL && tce != tce_deleted && tce != &empty_tce;
}

static inline bool unoccupied_tce(tcache_entry* tce) {
    return tce == NULL || tce == tce_deleted;
}

// lru counter is 32 bits, and is incremented for every frame.
// Only 31 bits are effective for comparisoin, MS bit is "set" if locked
// rollover @60 Hz -> 67 years,  @120 Hz -> 33 years
// so should be sufficient.
static uint32_t lru_counter = 1;
unsigned num_texture_bytes = 0;
unsigned max_num_texture_bytes = 0;
unsigned char delete_requested = 0;

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

static SDL_threadID delete_lock = 0;

// try acquire the spin lock, returns true if the lock was acquired,
// false otherwise.
static bool tcache_lock_try(SDL_threadID* ptr) {
    SDL_threadID expected = 0;
    SDL_threadID desired = SDL_GetThreadID(NULL);
    return __atomic_compare_exchange (ptr, &expected, &desired, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

// lock implementation using atomic compare and exchange
// The thread trying to acquire the lock "spins",
// sleeping for 1 millisecond and trying again.
static void tcache_lock(SDL_threadID* ptr) {
    while (!tcache_lock_try(ptr)) {
        if (*ptr == SDL_GetThreadID(NULL)) {
            error_printf("recursive locking is unsupported\n");
            // bug: state is uncertain terminate execution immediately
            exit(EXIT_FAILURE);
        }
        sleep_milli_seconds(1);
    }
}

// release previously acquired spin lock.
static void tcache_unlock(SDL_threadID* ptr) {
    SDL_threadID expected = SDL_GetThreadID(NULL);
    SDL_threadID desired = 0;
    if (__atomic_compare_exchange (ptr, &expected, &desired, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        return;
    }
    error_printf("tcache_unlock: invoked by thread not holding the lock\n");
    // bug: state is uncertain terminate execution immediately
    exit(EXIT_FAILURE);
}

void tcache_set_renderer_tid(const SDL_threadID tid) {
    tcache_init();
    if (renderer_tid == 0) {
        renderer_tid = tid; 
    } else {
        error_printf("Changing renderer thread ID is unsupported\n");
        // bug: state is uncertain terminate execution immediately
        exit(EXIT_FAILURE);
    }
}

// check for operations only permitted in the renderer thread context 
static inline bool check_permitted() {
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
        // set MSB if locked, since locked => newest always
        // setting the MSB is sufficient for all practical purposes.
        uint32_t effective_lru = tce_arr[j]->lru_count + tce_arr[j]->locked ? 1<<31: 0;
        uint64_t pivot_effective_lru = pivot->lru_count + pivot->locked ? 1<<31: 0;

        // if the current element <= pivot 
//        if (tce_arr[j]->lru_count <= pivot->lru_count) {
        if (effective_lru < pivot_effective_lru) {
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

//static inline void recently_used(tcache_entry* tce) {
//    if (tce) {
//        __atomic_store_n(&tce->lru_count, lru_counter, __ATOMIC_RELEASE);
////        tcache_printf("recently_used: tce=%p %ld %s\n", tce, tce->lru_count, tce->path);
//    } else {
//        error_printf("recently_used: tce=%p\n", tce);
//    }
//}

static void release_texture(tcache_entry* tce) {
    assert(external_tce(tce));
    if (external_tce(tce) && tce->texture) {
        int64_t ms_0 = get_micro_seconds();
        SDL_DestroyTexture((SDL_Texture*)tce->texture);
        int64_t ms_1 = get_micro_seconds();
//        perf_printf("release_texture: destroy_texture: %07.2f millis\n", (float)(ms_1 - ms_0)/1000);
        tce->texture = NULL;
        num_texture_bytes -= tce->num_bytes;
        tce->w = tce->h = tce->num_bytes = 0;
        profile_texture_printf("release_texture: destroy_texture: %06lu usec %u/%u\n", ms_1 - ms_0, num_texture_bytes, max_num_texture_bytes);
        tcache_printf("release_texture: texture_bytes=%d\n", num_texture_bytes);
    }
}

static void update_texture(tcache_entry* tce, const SDL_Texture* texture) {
    assert(external_tce(tce));
    if (external_tce(tce)) {
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

// custom string compare to handle NULL string pointers robustly
int compare_tce_paths(const char* path1, const char* path2) {
    if (path1 == path2) {
        return 0;
    }
    if (path1 == NULL || path2 == NULL) {
        return -1;
    }
    return strcmp(path1, path2);
}

texture_id_t tcache_create_entry(const char* path) {
    if (path == NULL) {
        error_printf("tcache_create_entry: path pointer is NULL\n");
        exit(EXIT_FAILURE);
    }
    uint32_t hashv = hashfn(path);
    texture_id_t indx = hashv%HASHTPRIME;
    int hop_count = 0;

    tcache_init();
    for(int count=0; count < HASHTPRIME; ++count) {
        tcache_entry** expected = tbl + indx;
        tcache_entry* tce = *expected;
        // index 0 is reserved for client side uninitialised texture id
        if (indx) {
            if (unoccupied_tce(tce)) {
                tce = calloc(1, sizeof(*tce));
                if (tce == NULL) {
                    error_printf("tcache_put_texture: Out of memory\n");
                    exit(EXIT_FAILURE);
                }
                tce->path = strdup(path);
                tce->hashv = hashv;
                __atomic_store_n(&tce->texture, NULL, __ATOMIC_RELEASE);
                tcache_entry** ptr = tbl + indx;
                tcache_entry** desired = &tce;
                if (__atomic_compare_exchange (ptr, expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
                    tcache_printf("tcache_create_texture: new: tce=%p %d %s\n", tce, indx, tce->path);
                    return indx;
                }
                // unable to add the entry, release and try again
                free(tce);
                tce = NULL;
                if (0 == compare_tce_paths(path, (*expected)->path)) {
                    // another thread has created the entry concurrently
                    tce = *expected;
                    return indx;
                }
                // another entry was added to the candidate slot continue searching for a free slot.
            } else {
                if (tce->hashv == hashv && 0 == compare_tce_paths(path,  tce->path)) {
                    tcache_lock(&delete_lock);
                    if (unoccupied_tce(tce)) {
                        // entry was deleted whilst acquiring the lock, probe the entry again
                        --count;
                    } else {
                        tcache_printf("tcache_create_texture: found: tce=%p %d %s\n", tce, indx, tce->path);
                        // ensure that the entry is not deleted
                        tce->delete = false;
                        tcache_unlock(&delete_lock);
                        return indx;
                    }
                    tcache_unlock(&delete_lock);
                }
            }
        }
        tcache_printf("tcache_create_texture: collision: hop:%d %d %u %s %s\n",
               hop_count, indx, hashv, path, tbl[indx]->path);
        indx = (indx+COLLISION_STEP)%HASHTPRIME;
        ++hop_count;
    }
    
    return -1;
}

// Get texture using the path/token
// path: path to image file - or unique string identifier
// texture_id*: quick access texture ID
// returns: texture, NULL is the texture is not found
//          texture ID or INVALID_TEXTURE_ID is texture is not found
SDL_Texture* tcache_get_texture(const char* path, texture_id_t* texture_id, SDL_Renderer* renderer) {
    if (!check_permitted()) {
        return NULL;
    }
    uint32_t hashv = hashfn(path);
    texture_id_t indx = hashv%HASHTPRIME;
    tcache_entry* tce = tbl[indx];

    for(int count=0; count < HASHTPRIME; ++count) {
        if (!unoccupied_tce(tce) && (tce->hashv == hashv && 0 == compare_tce_paths(path,  tce->path))) {
//            tcache_printf("tcache_get_texture: OK: %s\n", tce->path);
            if (texture_id) {
                *texture_id = indx;
            }
            return tcache_quick_get_texture(indx, renderer);
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
SDL_Texture* tcache_quick_get_texture(texture_id_t texture_id, SDL_Renderer* renderer) {
    if (!check_permitted()) {
        return NULL;
    }
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_quick_get_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (!unoccupied_tce(tce)) {
//        tcache_printf("tcache_quick_get_texture: %d %u %s\n", texture_id, tce->hashv, tce->path);
        __atomic_store_n(&tce->lru_count, lru_counter, __ATOMIC_RELEASE);

        if (tce->texture == NULL && tce->surface != NULL) {
            int64_t ms_ct_0 =get_micro_seconds();
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, tce->surface);
            int64_t ms_ct_1 =get_micro_seconds();
//            perf_printf("texture_resolve: create_texture: %07.2f millis\n", (float)(ms_ct_1 - ms_ct_0)/1000);
            if (NULL == texture) {
                error_printf("tcache_quick_get_texture: failed: %s %s\n", texture_id, tce->path, SDL_GetError());
                SDL_ClearError();
            }
            update_texture(tce, texture);
            SDL_FreeSurface(tce->surface);
            tce->surface = NULL;
            profile_texture_printf("texture_resolve: create_texture: %06lu usec %u/%u\n", ms_ct_1 - ms_ct_0, num_texture_bytes, max_num_texture_bytes);
        }
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
    if (external_tce(tce)) {
        return tce->ejected;
    }
    error_printf("tcache_quick_get_texture_ejected: none: %d\n", texture_id);
    return false;
}

static void _delete_texture(texture_id_t texture_id) {
    tcache_entry* tce = tbl[texture_id];
    assert(external_tce(tce));
    if (external_tce(tce)) {
        tcache_printf("tcache_quick_delete_texture: %d %p\n", texture_id, tce);
        release_texture(tce);
        if (tce->path) {
            free((void *)tce->path);
        }
        free(tce);
        __atomic_store_n(tbl+texture_id, tce_deleted, __ATOMIC_RELEASE);
    }
}

// Delete texture 
// texture_id*: quick access texture ID
bool tcache_quick_delete_texture(texture_id_t texture_id) {
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_quick_delete_texture: invalid id: %d\n", texture_id);
        exit(EXIT_FAILURE);
        return false;
    }
    tcache_entry* tce = tbl[texture_id];
    if (tce == tce_deleted) {
        return true;
    }
    if (external_tce(tce)) {
        tcache_lock(&delete_lock);
        tce->delete = true;
        __atomic_test_and_set(&delete_requested, __ATOMIC_ACQ_REL);
        tcache_unlock(&delete_lock);
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
        if (tce && (tce->hashv == hashv && 0 == compare_tce_paths(path,  tce->path))) {
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
        tcache_entry* tce =  __atomic_load_n(tbl +ix, __ATOMIC_ACQUIRE);
        // candidates for ejection must have a texture
        if (external_tce(tce) 
                && 
                __atomic_load_n(&tce->texture, __ATOMIC_ACQUIRE)) {
           lru_sorted_tbl[indx] = tce;
           ++indx;
        }
    }
    quick_sort_tcache(lru_sorted_tbl, indx);
    return indx;
}

static bool cap_exceeded(int increment, int ejected) {
    return max_num_texture_bytes && (num_texture_bytes + increment) > max_num_texture_bytes;
}

static struct {
    tcache_entry* tbl[HASHTPRIME];
    int count;
    int ix;
} lru_eject;

// Eject least recently used textures to reduce texture bytes to the configured limit
// TODO: this potentially expensive function in terms of time is called within the
// context of the renderer thread.
// Devise a scheme where entries are marked for ejection in the context of other threads
// and the renderer thread merely performs the release.
static bool tcache_eject(unsigned increment, bool (*check)(int, int)) {
    int64_t ms_0 = get_micro_seconds();
//    static tcache_entry* eject_tbl[HASHTPRIME];
    int ejected_count = 0;
//    int64_t ms_ct_0 = get_micro_seconds();
//    int count = lru_sort_tce(eject_tbl);
//    int64_t ms_ct_1 = get_micro_seconds();
//    profile_texture_printf("tcache_eject: lru_sort_tcache: %06lu\n", ms_ct_1 - ms_ct_0);
//    for(int ix=0; ix < count && check(increment, ejected_count); ++ix) {
    for(; lru_eject.ix < lru_eject.count && check(increment, ejected_count); ++lru_eject.ix) {
        tcache_entry* tce = lru_eject.tbl[lru_eject.ix];
        if (!tce->locked && tce->texture) {
            release_texture(tce);
            tce->ejected = true;
            ++ejected_count;
            tcache_eject_printf("tcache_eject: %s %d %u / %u\n", tce->path, increment, num_texture_bytes, max_num_texture_bytes);
        }
    }
    assert(lru_eject.ix < lru_eject.count);
    int64_t ms_1 = get_micro_seconds();
    profile_texture_printf("tcache_eject: %06lu usec ejected %d\n", ms_1- ms_0, ejected_count);
//    perf_printf("tcache_eject: %f millis\n", (float)(ms_1 - ms_0)/1000);
    return ejected_count;
}

// Eject least recently used textures to reduce texture bytes to the configured limit
static void tcache_cap_num_bytes(unsigned increment) {
    if( max_num_texture_bytes && (num_texture_bytes + increment) > max_num_texture_bytes ) {
        tcache_eject(increment, cap_exceeded);
    }
}

static bool test_cap_exceeded(int increment, int ejected_count) {
    return ejected_count == 0;
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
    // empty entry: with nothing to do, no file and texture is associated with this entry
    if (tce == &empty_tce) {
        return true;
    }
    tcache_lock(&delete_lock);
    bool tce_is_valid = !unoccupied_tce(tce);
    if (tce_is_valid) {
        // prevent the entry from being deleted.
        tce->delete = false;
    }
    tcache_unlock(&delete_lock);
    if (tce_is_valid) {
        // loading is only required if the associated texture or surface does not exist
        if (tce->texture == NULL && tce->surface == NULL) {
            tcache_printf("tcache_load_from_file: : %d %s\n", texture_id, tce->path);
            tce->surface = IMG_Load(tce->path);
            if (tce->surface == NULL)  {
                error_printf("tcache_load_from_file: failed: %d %s\n", texture_id, tce->path);
            } else {
                tce->w = tce->surface->w;
                tce->h = tce->surface->h;
            }
        }
        return tce->texture != NULL || tce->surface !=NULL; 
    }
    error_printf("tcache_load_from_file: invalid: %d\n", texture_id);
    return false;
}

// Some textures are generated dynamically, (not loaded from files for example)
// from example status text etc.
// this function may stall if the previously set surface has not been,
// resolved by the renderer thread.
bool tcache_set_surface(texture_id_t texture_id, SDL_Surface* surface) {
    if (texture_id < 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_set_surface: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (external_tce(tce)) {
        if (tce->surface == NULL ) {
            tce->surface = surface;
            return true;
        } else {
            SDL_Surface *obsolete = __atomic_exchange_n(&tce->surface, surface, __ATOMIC_ACQ_REL);
            if (obsolete) {
                SDL_FreeSurface(obsolete);
            }
            return true;
        }
    } else {
        error_printf("tcache_set_surface: set surface for unset or internal entry id=%d tce=%p\n", texture_id, tce);
        // client bug: for now exit
        exit(EXIT_FAILURE);
        return false;
    }
    return false;
} 

// Load texture from file and add it to the texture cache.
// path : path to image file
// renderer : SDL renderer context
// returns: texture ID
texture_id_t tcache_load_media(const char* path, SDL_Renderer* renderer, bool* ploaded) {
    texture_id_t texture_id = tcache_create_entry(path);
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
            if (tce && tce != tce_deleted) {
                printf("    %05d) delta=%4d hashv=%08x inuse=%016x %s tce=%p surface:%p texture=%p w=%4d h=%4d bytes=%8d %s\n",
                       ix, ix - last_ix,
                       tce->hashv,
                       tce->lru_count,
                       tce->locked ? "locked  ": "unlocked",
                       tce,
                       tce->surface,
                       tce->texture,
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
                printf("    %5d) hashv=%08x inuse=%016x %p %s\n", ix,
                       tce->hashv,
                       tce->lru_count,
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
    // locking the 0th entry, "uninitialised" is a client bug
    if (texture_id <= 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_lock_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (external_tce(tce)) {
        tce->locked = true;
    }
    return external_tce(tce);
}

bool tcache_unlock_texture(texture_id_t texture_id) {
    // locking the 0th entry, "uninitialised" is a client bug
    if (texture_id <= 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_unlock_texture: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (external_tce(tce)) {
        tce->locked = false;
    }
    return external_tce(tce);
}

// Get the texture id matching a token
// This function can be expensive in terms of execution time,
// especially if the token does not exist
texture_id_t tcache_get_texture_id(const char* token) {
    uint32_t hashv = hashfn(token);
    texture_id_t indx = hashv%HASHTPRIME;

    for(int count=0; count < HASHTPRIME; ++count, ++indx) {
        tcache_entry* tce = tbl[indx];
        if (!unoccupied_tce(tce) && (tce->hashv == hashv && 0 == compare_tce_paths(token,  tce->path))) {
            if (indx == 0) {
                error_printf("tcache_get_texture_id: internal error: matched with texture id 0\n");
                // bug in texture cache.
                exit(EXIT_FAILURE);
            }
            return indx;
        }
    }
    return INVALID_TEXTURE_ID;
}

static void _tcache_flush_textures(SDL_Renderer* renderer) {
    // When an image is deleted the delete request counter is incremented
    // When deletes are performed the delete done counter is synchronised with the req counter
    // since *all* deletes are performed.
    if (delete_requested) {
        // don't block on failure to acquire the lock, just return
        if (!tcache_lock_try(&delete_lock)) {
            return;
        }

        int64_t ms_0 = get_micro_seconds();
        // BEFORE deleting, clear the delete_requested flag,
        // then if another thread requests a texture delete during the scan of cached entries
        // for textures to delete:
        // 1) if the cache entry has been scanned already it will be deleted in the next invocation
        // of this function.
        // OR
        // 2) if the cache entry has not been scanned yet, the this loop may execute once more,
        // with no actual work (to be) done.
        __atomic_clear(&delete_requested, __ATOMIC_RELEASE);
        // texture_id 0 is the "unintialised" entry, skip it
        for(texture_id_t texture_id=0+1; texture_id < HASHTPRIME; ++texture_id) {
            tcache_entry* tce = tbl[texture_id];
            if (tce && tce->delete) {
                _delete_texture(texture_id);
            }
        }
        int64_t ms_1 = get_micro_seconds();
        profile_texture_printf("texture_flush: %06lu usec\n", ms_1 - ms_0);

        tcache_unlock(&delete_lock);
    }
}

void tcache_flush_textures(SDL_Renderer* renderer) {
    if (!check_permitted()) {
        return;
    }
    _tcache_flush_textures(renderer);
}

// texture cache prep for render must be called by the render thread,
// before each frame render.
void tcache_render_prep(SDL_Renderer* renderer) {
    if (!check_permitted()) {
        return;
    }
    _tcache_flush_textures(renderer);
   
    // bump the LRU counter
    __atomic_add_fetch(&lru_counter, 1, __ATOMIC_ACQ_REL);
    // setup for lru cache ejection,
//    int64_t ms_sort_0 = get_micro_seconds();
    lru_eject.count = lru_sort_tce(lru_eject.tbl);
    lru_eject.ix = 0;
//    int64_t ms_sort_1 = get_micro_seconds();
//    profile_texture_printf("lru_sort_tcache: %06lu\n", ms_sort_1 - ms_sort_0);
}

// Get texture width and height 
// texture_id*: quick access texture ID
// returns: true if the texture dimensions could be determined
bool tcache_quick_get_texture_dimensions(texture_id_t texture_id, int* w, int* h) {
    // accessing the 0th entry, "uninitialised" is a client bug
    if (texture_id <= 0 || texture_id >= NUM_TBL_ENTRIES) {
        error_printf("tcache_quick_get_texture_ejected: invalid id %d\n", texture_id);
        exit(EXIT_FAILURE);
    }
    tcache_entry* tce = tbl[texture_id];
    if (!unoccupied_tce(tce)) {
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
