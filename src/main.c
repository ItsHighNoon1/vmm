#include "vmm.h"

#include <stdio.h>
#include <stdlib.h>

#include <emscripten.h>
#include <SDL3/SDL.h>

void loop() {
    interface_tick();
    physics_tick();
    render_render();
}

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        printf("Failed SDL_Init\n");
        exit(1);
    }
    
    // Render init includes the SDL window so it has to happen before the interface
    render_init();
    interface_init();
    physics_init();

    // Imagine while(1) { loop() };
    emscripten_set_main_loop(loop, 0, true);

    physics_teardown();
    interface_teardown();
    render_teardown();
}