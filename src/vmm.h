#pragma once

#include <box2d/box2d.h>
#include <SDL3/SDL.h>

// Types
typedef enum {
    UNDEFINED = 0,
    MENU,
    DRAWING,
    SIMULATING,
} GameState_t;

typedef enum {
    UNFETCHED = 0,
    INPROGRESS,
    FETCHED,
    FAILED,
} LoadState_t;

#define PART_FLAGS_LAYER_0 0x01
#define PART_FLAGS_LAYER_1 0x02
#define PART_FLAGS_MOVABLE 0x04
#define PART_FLAGS_EDITABLE 0x08

typedef struct {
    int flags;
    int n_vertices;
    float* vertices;
} Part_t;

typedef struct {
    float x;
    float y;
    float force;
    float torque;
} Bolt_t;

typedef struct {
    union {
        SDL_Texture* texture;
        int http_code;
    };
    LoadState_t load_state;
} Texture_t;

// State global
extern GameState_t game_state;

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
void interface_render();
void interface_teardown();

// Physics functions
void physics_init();
void physics_tick();
void physics_build(int n_parts, Part_t* parts, int n_bolts, Bolt_t* bolts);
void physics_render();
void physics_teardown();

// Util functions
void load_texture_async(Texture_t* slot, char* url);
