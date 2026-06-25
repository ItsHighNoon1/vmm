#include "vmm.h"

#include <stdio.h>
#include <stdlib.h>

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

void render_init() {
    // Needed to attach to the correct canvas
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#canvas");
    if (!SDL_CreateWindowAndRenderer(NULL, 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        printf("Failed SDL_CreateWindowAndRenderer\n");
        exit(1);
    }
    SDL_SetRenderLogicalPresentation(renderer, 16, 9, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderScale(renderer, 0.03f, 0.03f); // Makes the lines look a good size
}

void render_render() {
    switch (game_state) {
        case DRAWING:
            interface_render();
            break;
        case SIMULATING:
            //SDL_SetRenderDrawColor(renderer, 0xF2, 0xDC, 0xB1, SDL_ALPHA_OPAQUE);
            //SDL_RenderClear(renderer);
            break;
        default:
            break;
    }
    
    SDL_RenderPresent(renderer);
}

void render_teardown() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}