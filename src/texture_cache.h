#ifndef  __TEXTURE_H_
#include <SDL2/SDL.h>
#include "types.h"
typedef int  texture_id_t;

texture_id_t tcache_put_texture(const char* token, const SDL_Texture* texture, bool ejectable);
bool tcache_set_texture(texture_id_t texture_id, SDL_Texture* texture);
const SDL_Texture* tcache_get_texture(const char* token, texture_id_t* texture_id);
const SDL_Texture* tcache_quick_get_texture(texture_id_t texture_id);
bool tcache_quick_get_texture_ejected(texture_id_t texture_id);
bool tcache_quick_delete_texture(texture_id_t texture_id);
bool tcache_delete_texture(const char* token);
bool tcache_eject_lru();
bool tcache_load_from_file(texture_id_t texture_id, SDL_Renderer* renderer);
texture_id_t tcache_load_media(const char* path, SDL_Renderer* renderer, bool ejectable);
void tcache_dump();
#endif
