#ifndef  __TEXTURE_H_
#include <SDL2/SDL.h>
#include "types.h"
typedef int  texture_id_t;
#define INVALID_TEXTURE_ID -1

void tcache_init(void);
texture_id_t tcache_put_texture(const char* token, const SDL_Texture* texture);
bool tcache_set_texture(texture_id_t texture_id, const SDL_Texture* texture);
SDL_Texture* tcache_get_texture(const char* token, texture_id_t* texture_id);
SDL_Texture* tcache_quick_get_texture(texture_id_t texture_id);
bool tcache_quick_get_texture_ejected(texture_id_t texture_id);
bool tcache_quick_delete_texture(texture_id_t texture_id);
bool tcache_delete_texture(const char* token);
bool tcache_eject_lru();
bool tcache_load_from_file(texture_id_t texture_id, SDL_Renderer* renderer);
texture_id_t tcache_load_media(const char* path, SDL_Renderer* renderer, bool* loaded);
void tcache_dump();
int64_t tcache_get_texture_bytes_count(void);
texture_id_t tcache_get_empty_tid(void);

bool tcache_lock_texture(texture_id_t texture_id);
bool tcache_unlock_texture(texture_id_t texture_id);
texture_id_t tcache_get_texture_id(const char* token);

// Must be called in the thread that created the renderer
void tcache_resolve_textures(SDL_Renderer* renderer);

bool tcache_quick_get_texture_dimensions(texture_id_t texture_id, int* w, int* h);
void tcache_set_limit(int);
#endif
