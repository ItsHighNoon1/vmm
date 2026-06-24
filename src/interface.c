#include "vmm.h"

#include <stdio.h>

#include <emscripten.h>
#include <SDL3/SDL.h>

void interface_init() {
    SDL_StartTextInput(window);
}

void interface_tick() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                //emscripten_cancel_main_loop();
                return;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                printf("mouse down\n");
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                printf("mouse up\n");
                break;
            case SDL_EVENT_MOUSE_MOTION:
                printf("mouse moved %f %f\n", event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_KEY_DOWN:
                printf("key down %c\n", event.key.key);
                break;
        }
    }
}

void interface_teardown() {
    SDL_StopTextInput(window);
}