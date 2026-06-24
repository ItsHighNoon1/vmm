#include <stdio.h>
#include <stdlib.h>

#include <emscripten.h>
#include <SDL3/SDL.h>

#include "render.h"

#include <box2d/box2d.h>

void loop() {
    render_render();
}

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        printf("Failed SDL_Init\n");
        exit(1);
    }
    
    render_init();

    emscripten_set_main_loop(loop, 0, true);

    render_teardown();
}