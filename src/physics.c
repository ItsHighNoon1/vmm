#include "box2d/types.h"
#include "vmm.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <box2d/box2d.h>
#include <emscripten.h>

typedef struct {
    union {
        struct {
            union {
                struct {
                    float x;
                    float y;
                };
                b2Vec2 vec;
            };
        } p[3];
        float v[3][2];
    };
} Triangle_t;

typedef struct {
    Part_t* source_part;
    b2BodyId body_id;
    Triangle_t* triangles;
    int n_triangles;
    int n_bolts_to_wall;
} PhysicsObject_t;

typedef struct {
    PhysicsObject_t** parts;
    int count;
} BoltQueryParms_t;

b2WorldId world_id;
PhysicsObject_t* bodies;
int n_bodies;
int s_bodies;
Bolt_t* saved_bolts;
int n_saved_bolts;

static bool bolt_query_callback(b2ShapeId shape_id, void* user) {
    BoltQueryParms_t* bolt_query_parms = (BoltQueryParms_t*)user;
    if (bolt_query_parms->parts) {
        b2BodyId body_id = b2Shape_GetBody(shape_id);
        bolt_query_parms->parts[bolt_query_parms->count] = (PhysicsObject_t*)b2Body_GetUserData(body_id);
    }
    bolt_query_parms->count++;
    return true;
}

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

static bool is_in_triangle(Triangle_t* t, float v_x, float v_y) {
    float d1 = (v_x - t->p[1].x) * (t->p[0].y - t->p[1].y) - (t->p[0].x - t->p[1].x) * (v_y - t->p[1].y);
    float d2 = (v_x - t->p[2].x) * (t->p[1].y - t->p[2].y) - (t->p[1].x - t->p[2].x) * (v_y - t->p[2].y);
    float d3 = (v_x - t->p[0].x) * (t->p[2].y - t->p[0].y) - (t->p[2].x - t->p[0].x) * (v_y - t->p[0].y);
    bool has_negative = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    bool has_positive = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(has_negative && has_positive);
}

static void triangulate(int n_vertices, float* vertices, int* n_triangles, Triangle_t** triangles) {
    // https://devforum.roblox.com/t/triangulating-polygons-using-the-ear-clipping-algorithm/1875449
    if (n_vertices < 3 || vertices == NULL || n_triangles == NULL || triangles == NULL) {
        return;
    }
    *n_triangles = 0;
    *triangles = malloc((n_vertices - 2) * sizeof(Triangle_t));

    float working_vertices[2 * n_vertices];
    int n_working_vertices = n_vertices;

    // We need to know the winding order for triangulation, so we will flip the vertices if we
    // find that the polygon is clockwise winding order
    float ccw_test = 0.0f;
    for (int edge_idx = 0; edge_idx < n_vertices; edge_idx++) {
        float x1 = vertices[2 * edge_idx + 0];
        float y1 = vertices[2 * edge_idx + 1];
        float x2 = vertices[2 * ((edge_idx + 1) % n_vertices) + 0];
        float y2 = vertices[2 * ((edge_idx + 1) % n_vertices) + 1];
        ccw_test += (x2 - x1) * (y2 + y1);
    }
    if (ccw_test > 0.0f) {
        for (int v_idx = 0; v_idx < n_vertices; v_idx++) {
            int working_idx = n_vertices - v_idx - 1;
            working_vertices[2 * working_idx + 0] = vertices[2 * v_idx + 0];
            working_vertices[2 * working_idx + 1] = vertices[2 * v_idx + 1];
        }
    } else {
        memcpy(working_vertices, vertices, 2 * n_vertices * sizeof(float));
    }

    for (int tri_idx = 0; tri_idx < n_vertices - 2; tri_idx++) {
        for (int a_idx = 0; a_idx < n_working_vertices; a_idx++) {
            int b_idx = (a_idx + 1) % n_working_vertices;
            int c_idx = (b_idx + 1) % n_working_vertices;
            float ax = working_vertices[2 * a_idx + 0];
            float ay = working_vertices[2 * a_idx + 1];
            float bx = working_vertices[2 * b_idx + 0];
            float by = working_vertices[2 * b_idx + 1];
            float cx = working_vertices[2 * c_idx + 0];
            float cy = working_vertices[2 * c_idx + 1];
            float d = (bx - ax) * (cy - by) - (by - ay) * (cx - bx);
            if (d <= 0.0f) {
                // This is an exterior triangle
                continue;
            }
            Triangle_t triangle = {
                { ax, ay, 
                  bx, by,
                  cx, cy }
            };
            bool contains_point = false;
            for (int p_idx = 0; p_idx < n_working_vertices; p_idx++) {
                if (p_idx == a_idx || p_idx == b_idx || p_idx == c_idx) {
                    continue;
                }
                float px = working_vertices[2 * p_idx + 0];
                float py = working_vertices[2 * p_idx + 1];
                if (is_in_triangle(&triangle, px, py)) {
                    contains_point = true;
                    break;
                }
            }
            if (contains_point) {
                continue;
            }

            // Ear can be added to the triangulation
            memcpy(&(*triangles)[(*n_triangles)++], &triangle, sizeof(Triangle_t));

            // Vertex B needs to be removed from the working vertex list
            for (int rm_idx = b_idx; rm_idx < n_working_vertices - 1; rm_idx++) {
                working_vertices[2 * rm_idx + 0] = working_vertices[2 * rm_idx + 2];
                working_vertices[2 * rm_idx + 1] = working_vertices[2 * rm_idx + 3];
            }
            n_working_vertices--;
        }
    }

    assert(*n_triangles == n_vertices - 2);
}

void physics_init() {
    world_id = b2_nullWorldId;
    bodies = NULL;
    n_bodies = 0;
    s_bodies = 0;
}

void physics_tick() {
    float dt = 1.0f / 30.0f;
    int sub_step_count = 8;
    if (b2World_IsValid(world_id)) {
        b2World_Step(world_id, dt, sub_step_count);
    }
}

void physics_build(int n_parts, Part_t* parts, int n_bolts, Bolt_t* bolts) {
    if (b2World_IsValid(world_id)) {
        b2DestroyWorld(world_id);
    }
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = (b2Vec2){ 0.0f, 30.0f };
    world_id = b2CreateWorld(&world_def);

    if (bodies == NULL) {
        s_bodies = 10;
        bodies = malloc(s_bodies * sizeof(PhysicsObject_t));
    } else {
        for (int body_idx = 0; body_idx < n_bodies; body_idx++) {
            free(bodies[body_idx].triangles);
        }
    }
    n_bodies = 0;

    // Part stage
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

        for (int tri_idx = 0; tri_idx < n_triangles; tri_idx++) {
            triangles[tri_idx].p[0].x -= center_x;
            triangles[tri_idx].p[0].y -= center_y;
            triangles[tri_idx].p[1].x -= center_x;
            triangles[tri_idx].p[1].y -= center_y;
            triangles[tri_idx].p[2].x -= center_x;
            triangles[tri_idx].p[2].y -= center_y;
        }

        // Set up physics object
        bodies[n_bodies].source_part = &parts[part_idx];
        b2BodyDef body_def = b2DefaultBodyDef();
        body_def.type = b2_dynamicBody;
        body_def.position = (b2Vec2){ center_x, center_y };
        bodies[n_bodies].body_id = b2CreateBody(world_id, &body_def);
        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.density = 1.0f;
        shape_def.material.friction = 0.3f;
        for (int tri_idx = 0; tri_idx < n_triangles; tri_idx++) {
            b2Hull triangle_hull = b2ComputeHull(&triangles[tri_idx].p[0].vec, 3);
            b2Polygon triangle_polygon = b2MakePolygon(&triangle_hull, 0.0f);
            b2CreatePolygonShape(bodies[n_bodies].body_id, &shape_def, &triangle_polygon);
        }
        bodies[n_bodies].triangles = triangles;
        bodies[n_bodies].n_triangles = n_triangles;
        bodies[n_bodies].n_bolts_to_wall = 0;
        b2Body_SetUserData(bodies[n_bodies].body_id, &bodies[n_bodies]);
        n_bodies++;
    }

    // Bolt state
    n_saved_bolts = n_bolts;
    saved_bolts = bolts;
    for (int bolt_idx = 0; bolt_idx < n_saved_bolts; bolt_idx++) {
        b2Vec2 bolt_pos = { bolts[bolt_idx].x, bolts[bolt_idx].y };
        b2ShapeProxy proxy = b2MakeProxy(&bolt_pos, 1, 0.0f);

        // Check how many shapes this bolt intersects
        BoltQueryParms_t parms = { NULL, 0 };
        b2World_OverlapShape(world_id, b2Pos_zero, &proxy, b2DefaultQueryFilter(), bolt_query_callback, &parms);
        if (parms.count == 0) {
            continue;
        }
        parms.parts = malloc(parms.count * sizeof(PhysicsObject_t*));
        parms.count = 0;
        
        b2World_OverlapShape(world_id, b2Pos_zero, &proxy, b2DefaultQueryFilter(), bolt_query_callback, &parms);
        if (parms.count == 1) {
            if (++parms.parts[0]->n_bolts_to_wall >= 2) {
                // 2 bolts makes it static
                b2Body_SetType(parms.parts[0]->body_id, b2_staticBody);
            } else {
                // Make a dummy body at the bolt position and create a joint
                // TODO
            }
        } else if (parms.count == 2) {

        } else {
            printf("Unsupported number of parts for one bolt: %d\n", parms.count);
        }
        free(parms.parts);
    }
}

void physics_render() {
    SDL_SetRenderDrawColor(renderer, 0xF2, 0xDC, 0xB1, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0x57, 0x57, 0x57, SDL_ALPHA_OPAQUE);
    for (int body_idx = 0; body_idx < n_bodies; body_idx++) {
        b2WorldTransform transform = b2Body_GetTransform(bodies[body_idx].body_id);
        for (int tri_idx = 0; tri_idx < bodies[body_idx].n_triangles; tri_idx++) {
            Triangle_t* triangle = &bodies[body_idx].triangles[tri_idx];
            b2Vec2 a = b2TransformWorldPoint(transform, triangle->p[0].vec);
            b2Vec2 b = b2TransformWorldPoint(transform, triangle->p[1].vec);
            b2Vec2 c = b2TransformWorldPoint(transform, triangle->p[2].vec);
            SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);
            SDL_RenderLine(renderer, b.x, b.y, c.x, c.y);
            SDL_RenderLine(renderer, c.x, c.y, a.x, a.y);
        }
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    for (int bolt_idx = 0; bolt_idx < n_saved_bolts; bolt_idx++) {
        SDL_RenderLine(renderer,
            saved_bolts[bolt_idx].x - 2.0f, saved_bolts[bolt_idx].y,
            saved_bolts[bolt_idx].x + 2.0f, saved_bolts[bolt_idx].y);
        SDL_RenderLine(renderer,
            saved_bolts[bolt_idx].x, saved_bolts[bolt_idx].y + 2.0f,
            saved_bolts[bolt_idx].x, saved_bolts[bolt_idx].y - 2.0f);
    }
    SDL_RenderPresent(renderer);
}

void physics_teardown() {
    b2DestroyWorld(world_id);
}