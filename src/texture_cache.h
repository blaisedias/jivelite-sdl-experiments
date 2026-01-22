#ifndef  __jl_texture_h_
#define __jl_texture_h_
#include <SDL2/SDL.h>
#include "types.h"
typedef int  texture_id_t;
#define INVALID_TEXTURE_ID -1

void tcache_init(void);
void tcache_set_renderer_tid(const SDL_threadID);

// { These functions must be called in the thread that created the renderer
void tcache_resolve_textures(SDL_Renderer* renderer);
texture_id_t tcache_put_texture(const char* token, const SDL_Texture* texture);
bool tcache_set_texture(texture_id_t texture_id, const SDL_Texture* texture);
SDL_Texture* tcache_get_texture(const char* token, texture_id_t* texture_id);
SDL_Texture* tcache_quick_get_texture(texture_id_t texture_id);
bool tcache_quick_get_texture_ejected(texture_id_t texture_id);
bool tcache_quick_delete_texture(texture_id_t texture_id);
bool tcache_delete_texture(const char* token);

// Test only function
bool tcache_test_lru_eject();
// }

// These functions can be called by any thread
bool tcache_load_from_file(texture_id_t texture_id, SDL_Renderer* renderer);
texture_id_t tcache_load_media(const char* path, SDL_Renderer* renderer, bool* loaded);

bool tcache_lock_texture(texture_id_t texture_id);
bool tcache_unlock_texture(texture_id_t texture_id);

texture_id_t tcache_get_empty_tid(void);
texture_id_t tcache_get_texture_id(const char* token);

bool tcache_quick_get_texture_dimensions(texture_id_t texture_id, int* w, int* h);

unsigned tcache_get_texture_bytes_count(void);
void tcache_set_limit(unsigned);

// Diagnostics
void tcache_dump();

#endif // __jl_texture_h_
