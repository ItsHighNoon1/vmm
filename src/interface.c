#include "vmm.h"

#include <stdio.h>

#include <emscripten.h>
#include <SDL3/SDL.h>

#define MIN_DISTANCE 50

GameState_t game_state;
Part_t* parts;
Bolt_t* bolts;
int n_parts;
int s_parts;
int n_bolts;
int s_bolts;
float last_mouse_x;
float last_mouse_y;
float* vertex_draw_list;
int n_vertex_draw_list;
int s_vertex_draw_list;

static void transition_to_drawing() {
    printf("Switched to drawing mode\n");
    game_state = DRAWING;
}

static void transition_to_simulation() {
    printf("Switched to simulation mode\n");
    game_state = SIMULATING;
    physics_build(n_parts, parts, n_bolts, bolts);
}

void interface_init() {
    SDL_StartTextInput(window);
    game_state = DRAWING;
    n_parts = 0;
    s_parts = 10;
    parts = malloc(s_parts * sizeof(Part_t));
    n_bolts = 0;
    s_bolts = 10;
    bolts = malloc(s_bolts * sizeof(Bolt_t));
    last_mouse_x = 0.0f;
    last_mouse_y = 0.0f;
}

void interface_tick() {
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        SDL_ConvertEventToRenderCoordinates(renderer, &event);
        switch (event.type) {
            case SDL_EVENT_QUIT:
                emscripten_cancel_main_loop();
                return;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (game_state != DRAWING || event.button.button != SDL_BUTTON_LEFT) {
                    break;
                }
                if (vertex_draw_list != NULL) {
                    // ???
                    free(vertex_draw_list);
                }
                n_vertex_draw_list = 0;
                s_vertex_draw_list = 10;
                vertex_draw_list = malloc(2 * s_vertex_draw_list * sizeof(float));
                last_mouse_x = event.button.x;
                last_mouse_y = event.button.y;
                vertex_draw_list[2 * n_vertex_draw_list + 0] = last_mouse_x;
                vertex_draw_list[2 * n_vertex_draw_list + 1] = last_mouse_y;
                n_vertex_draw_list++;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button != SDL_BUTTON_LEFT) {
                    break;
                }
                if (game_state != DRAWING || vertex_draw_list) {
                    if (n_parts >= s_parts) {
                        s_parts *= 2;
                        parts = realloc(parts, s_parts * sizeof(Part_t));
                    }
                    parts[n_parts].flags = PART_FLAGS_LAYER_0 | PART_FLAGS_MOVABLE | PART_FLAGS_EDITABLE;
                    parts[n_parts].vertices = realloc(vertex_draw_list, 2 * n_vertex_draw_list * sizeof(float));
                    parts[n_parts].n_vertices = n_vertex_draw_list;
                    n_parts++;
                }
                n_vertex_draw_list = 0;
                s_vertex_draw_list = 0;
                vertex_draw_list = NULL;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (game_state == DRAWING && event.motion.state & SDL_BUTTON_LEFT && vertex_draw_list) {
                    float dx = event.motion.x - last_mouse_x;
                    float dy = event.motion.y - last_mouse_y;
                    float dist2 = dx * dx + dy * dy;
                    if (dist2 > MIN_DISTANCE * MIN_DISTANCE) {
                        if (n_vertex_draw_list >= s_vertex_draw_list) {
                            s_vertex_draw_list *= 2;
                            vertex_draw_list = realloc(vertex_draw_list, 2 * s_vertex_draw_list * sizeof(float));
                        }
                        last_mouse_x = event.motion.x;
                        last_mouse_y = event.motion.y;
                        vertex_draw_list[2 * n_vertex_draw_list + 0] = last_mouse_x;
                        vertex_draw_list[2 * n_vertex_draw_list + 1] = last_mouse_y;
                        n_vertex_draw_list++;
                    }
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.scancode) {
                    case SDL_SCANCODE_SPACE:
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_RETURN2:
                        if (game_state == DRAWING) {
                            transition_to_simulation();
                        } else if (game_state == SIMULATING) {
                            transition_to_drawing();
                        }
                        break;
                    default:
                        break;
                }
                break;
        }
    }
}

void interface_render() {
    SDL_SetRenderDrawColor(renderer, 0, 0x56, 0xA3, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
    for (int part_idx = 0; part_idx < n_parts; part_idx++) {
        for (int vertex_idx = 0; vertex_idx < parts[part_idx].n_vertices; vertex_idx++) {
            float x1 = parts[part_idx].vertices[2 * vertex_idx + 0];
            float y1 = parts[part_idx].vertices[2 * vertex_idx + 1];
            float x2;
            float y2;
            if (vertex_idx + 1 == parts[part_idx].n_vertices) {
                x2 = parts[part_idx].vertices[0];
                y2 = parts[part_idx].vertices[1];
            } else {
                x2 = parts[part_idx].vertices[2 * vertex_idx + 2];
                y2 = parts[part_idx].vertices[2 * vertex_idx + 3];
            }
            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }
    }
    for (int vertex_idx = 0; vertex_idx < n_vertex_draw_list; vertex_idx++) {
        float x1 = vertex_draw_list[2 * vertex_idx + 0];
        float y1 = vertex_draw_list[2 * vertex_idx + 1];
        float x2;
        float y2;
        if (vertex_idx + 1 == n_vertex_draw_list) {
            x2 = vertex_draw_list[0];
            y2 = vertex_draw_list[1];
        } else {
            x2 = vertex_draw_list[2 * vertex_idx + 2];
            y2 = vertex_draw_list[2 * vertex_idx + 3];
        }
        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }
}

void interface_teardown() {
    SDL_StopTextInput(window);
}