#include "vmm.h"

#include <stdio.h>

static void _texture_load_success(unsigned int handle, void* user_data, const char* filename) {
    Texture_t* slot = (Texture_t*)user_data;
    
    SDL_Surface* surface = SDL_LoadPNG(filename);
    if (!surface) {
        printf("Texture load failure: %s\n", SDL_GetError());
        slot->load_state = FAILED;
        slot->http_code = -1;
        return;
    }
    slot->texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!slot->texture) {
        printf("Texture object creation failure: %s\n", SDL_GetError());
        slot->load_state = FAILED;
        slot->http_code = -2;
        return;
    }
    slot->load_state = FETCHED;
    SDL_DestroySurface(surface);
}

static void _texture_load_failure(unsigned int handle, void* user_data, int status) {
    Texture_t* slot = (Texture_t*)user_data;
    slot->load_state = FAILED;
    slot->http_code = status;
}

void load_texture_async(Texture_t* slot, char* url) {
    slot->load_state = INPROGRESS;
    char filename[16] = { 0 };
    int offset_of_last_bytes = strlen(url) - sizeof(filename);
    if (offset_of_last_bytes < 0) {
        offset_of_last_bytes = 0;
    }
    strncpy(filename, url + offset_of_last_bytes, sizeof(filename));
    emscripten_async_wget2(url, filename, "GET", NULL, (void*)slot, _texture_load_success, _texture_load_failure, NULL);
}
