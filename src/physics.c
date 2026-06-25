#include "vmm.h"

#include <stdlib.h>
#include <stdio.h>

#include <box2d/box2d.h>
#include <emscripten.h>

typedef struct {
    Part_t* source_part;
    b2BodyId body_id;
} PhysicsObject_t;

typedef struct {
    union {
        struct {
            float x;
            float y;
        } p[3];
        float v[6];
    };
} Triangle_t;

b2WorldId world_id;
PhysicsObject_t* bodies;
int n_bodies;
int s_bodies;

static bool is_in_circumcircle(Triangle_t* t, float v_x, float v_y) {
    // https://en.wikipedia.org/wiki/Circumcircle#Cartesian_coordinates_2
    // https://brandewinder.com/2025/03/19/delaunay-triangulation-circumcircle/
    // I never was good at this circle stuff, I hope LLVM is cracked
    float d = 2.0f * (
        t->p[0].x * (t->p[1].y - t->p[2].y) +
        t->p[1].x * (t->p[2].y - t->p[0].y) +
        t->p[2].x * (t->p[0].y - t->p[1].y));
    float x = 
        (t->p[0].x * t->p[0].x + t->p[0].y * t->p[0].y) * (t->p[1].y - t->p[2].y) +
        (t->p[1].x * t->p[1].x + t->p[1].y * t->p[1].y) * (t->p[2].y - t->p[0].y) +
        (t->p[2].x * t->p[2].x + t->p[2].y * t->p[2].y) * (t->p[0].y - t->p[1].y);
    float y = 
        (t->p[0].x * t->p[0].x + t->p[0].y * t->p[0].y) * (t->p[2].x - t->p[1].x) +
        (t->p[1].x * t->p[1].x + t->p[1].y * t->p[1].y) * (t->p[0].x - t->p[2].x) +
        (t->p[2].x * t->p[2].x + t->p[2].y * t->p[2].y) * (t->p[1].x - t->p[0].x);
    float cx = x / d;
    float cy = y / d;
    float r_dx = t->p[0].x - cx;
    float r_dy = t->p[0].y - cy;
    float r2 = r_dx * r_dx + r_dy * r_dy;
    float v_dx = v_x - cx;
    float v_dy = v_y - cy;
    float v_r2 = v_dx * v_dx + v_dy * v_dy;
    return v_r2 < r2;
}

static void triangulate(int n_vertices, float* vertices, int* n_triangles, Triangle_t** triangles) {
    // https://devforum.roblox.com/t/triangulating-polygons-using-the-ear-clipping-algorithm/1875449
    if (n_vertices < 3 || vertices == NULL || n_triangles == NULL || triangles == NULL) {
        return;
    }
    *n_triangles = n_vertices - 2;
    *triangles = malloc(*n_triangles * sizeof(Triangle_t));
    
    float working_vertices[2 * n_vertices];
    int n_working_vertices = n_vertices;
    memcpy(working_vertices, vertices, 2 * n_vertices * sizeof(float));

    for (int tri_idx = 0; tri_idx < *n_triangles; tri_idx++) {
        for (int a_idx = 0; a_idx < n_working_vertices; a_idx++) {
            int b_idx = (a_idx + 1) % n_working_vertices;
            int c_idx = (b_idx + 1) % n_working_vertices;
            float ax = working_vertices[2 * a_idx + 0];
            float ay = working_vertices[2 * a_idx + 1];
            float bx = working_vertices[2 * b_idx + 0];
            float by = working_vertices[2 * b_idx + 1];
            float cx = working_vertices[2 * c_idx + 0];
            float cy = working_vertices[2 * c_idx + 1];
            for (int p_idx = 0; p_idx < n_working_vertices; p_idx++) {
                if (p_idx == a_idx || p_idx == b_idx || p_idx == c_idx) {
                    continue;
                }
                float px = working_vertices[2 * p_idx + 0];
                float py = working_vertices[2 * p_idx + 1];
            }
        }
    }
}

void physics_init() {
    world_id = b2_nullWorldId;
    bodies = NULL;
    n_bodies = 0;
    s_bodies = 0;
}

void physics_tick() {
    float dt = 1.0f / 60.0f;
    int sub_step_count = 4;
    if (b2World_IsValid(world_id)) {
        b2World_Step(world_id, dt, sub_step_count);
    }
}

void physics_build(int n_parts, Part_t* parts, int n_bolts, Bolt_t* bolts) {
    if (b2World_IsValid(world_id)) {
        b2DestroyWorld(world_id);
    }
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = (b2Vec2){ 0.0f, -10.0f };
    world_id = b2CreateWorld(&world_def);

    n_bodies = 0;
    if (bodies == NULL) {
        s_bodies = 10;
        bodies = malloc(s_bodies * sizeof(PhysicsObject_t));
    }

    printf("Triangulating %d parts\n", n_parts);
    for (int part_idx = 0; part_idx < n_parts; part_idx++) {
        if (parts[part_idx].n_vertices <= 0) {
            continue;
        }

        if (n_bodies >= s_bodies) {
            s_bodies *= 2;
            bodies = realloc(bodies, s_bodies * sizeof(PhysicsObject_t));
        }

        // Evil triangulation algorithm
        Triangle_t* triangles;
        int n_triangles;
        triangulate(parts[part_idx].n_vertices, parts[part_idx].vertices, &n_triangles, &triangles);
        printf("Part %d triangulated to %d triangles\n", part_idx, n_triangles);
        if (n_triangles <= 0) {
            free(triangles);
            return;
        }

        // Compute center
        float min_x = parts[part_idx].vertices[0];
        float max_x = parts[part_idx].vertices[0];
        float min_y = parts[part_idx].vertices[1];
        float max_y = parts[part_idx].vertices[1];
        for (int v_idx = 1; v_idx < parts[part_idx].n_vertices; v_idx++) {
            if (parts[part_idx].vertices[2 * v_idx + 0] > max_x) {
                max_x = parts[part_idx].vertices[2 * v_idx + 0];
            } else if (parts[part_idx].vertices[2 * v_idx + 0] < min_x) {
                min_x = parts[part_idx].vertices[2 * v_idx + 0];
            }
            if (parts[part_idx].vertices[2 * v_idx + 1] > max_y) {
                max_y = parts[part_idx].vertices[2 * v_idx + 1];
            } else if (parts[part_idx].vertices[2 * v_idx + 1] < min_y) {
                min_y = parts[part_idx].vertices[2 * v_idx + 1];
            }
        }
        float center_x = (min_x + max_x) / 2.0f;
        float center_y = (min_y + max_y) / 2.0f;

        b2BodyDef body_def = b2DefaultBodyDef();
        body_def.type = b2_dynamicBody;
        body_def.position = (b2Vec2){ center_x, center_y };
        bodies[n_bodies].body_id = b2CreateBody(world_id, &body_def);
        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.density = 1.0f;
        shape_def.material.friction = 0.3f;
        //b2CreatePolygonShape(bodies[n_bodies].body_id, &shape_def, &dynamic_box);

        free(triangles);
    }
}

void physics_render() {

}

void physics_teardown() {
    b2DestroyWorld(world_id);
}