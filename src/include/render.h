#pragma once

#include <SDL3/SDL.h>

extern SDL_Window* window;
extern SDL_Renderer* renderer;

void render_init();
void render_render();
void render_teardown();