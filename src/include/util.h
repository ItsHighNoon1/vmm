#pragma once

#include <emscripten.h>
#include <SDL3/SDL.h>

typedef enum {
    UNFETCHED = 0,
    INPROGRESS,
    FETCHED,
    FAILED,
} State_t;

typedef struct {
    union {
        SDL_Texture* texture;
        int http_code;
    };
    State_t load_state;
} Texture_t;



void load_texture_async(Texture_t* slot, char* url);