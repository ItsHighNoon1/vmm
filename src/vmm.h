#pragma once

#include <SDL3/SDL.h>

// Types
typedef enum {
    UNFETCHED = 0,
    INPROGRESS,
    FETCHED,
    FAILED,
} LoadState_t;

typedef struct {
    union {
        SDL_Texture* texture;
        int http_code;
    };
    LoadState_t load_state;
} Texture_t;

// SDL globals
extern SDL_Window* window;
extern SDL_Renderer* renderer;

// Renderer functions
void render_init();
void render_render();
void render_teardown();

// Interface functions
void interface_init();
void interface_tick();
void interface_teardown();

// Physics functions
void physics_init();
void physics_tick();
void physics_teardown();

// Util functions
void load_texture_async(Texture_t* slot, char* url);
