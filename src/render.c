#include "render.h"

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
    SDL_SetRenderLogicalPresentation(renderer, 1, 1, SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void render_render() {
    SDL_SetRenderDrawColor(renderer, 0, 0x56, 0xA3, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_RenderPresent(renderer);
}

void render_teardown() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}