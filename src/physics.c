#include "vmm.h"

#include <SDL3/SDL_audio.h>
#include <assert.h>
#include <math.h>
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
    PhysicsObject_t* parent;
    b2Vec2 offset;
} PhysicsBolt_t;

typedef struct {
    PhysicsObject_t** parts;
    int count;
} BoltQueryParms_t;

b2WorldId world_id;
PhysicsObject_t* bodies;
int n_bodies;
int s_bodies;
PhysicsBolt_t* render_bolts;
int n_render_bolts;
Texture_t iron_texture;
Texture_t bolt_texture;

static bool bolt_query_callback(b2ShapeId shape_id, void* user) {
    BoltQueryParms_t* bolt_query_parms = (BoltQueryParms_t*)user;
    if (bolt_query_parms->parts) {
        b2BodyId body_id = b2Shape_GetBody(shape_id);
        PhysicsObject_t* found_part = (PhysicsObject_t*)b2Body_GetUserData(body_id);
        for (int part_idx = 0; part_idx < bolt_query_parms->count; part_idx++) {
            if (found_part == bolt_query_parms->parts[part_idx]) {
                // We already hit this part
                return true;
            }
        }
        bolt_query_parms->parts[bolt_query_parms->count++] = found_part;
    } else {
        bolt_query_parms->count++;
    }
    return true;
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
    // find that the polygon is counterclockwise winding order
    float ccw_test = 0.0f;
    for (int edge_idx = 0; edge_idx < n_vertices; edge_idx++) {
        float x1 = vertices[2 * edge_idx + 0];
        float y1 = vertices[2 * edge_idx + 1];
        float x2 = vertices[2 * ((edge_idx + 1) % n_vertices) + 0];
        float y2 = vertices[2 * ((edge_idx + 1) % n_vertices) + 1];
        ccw_test += (x2 - x1) * (y2 + y1);
    }
    if (ccw_test < 0.0f) {
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
            if (d >= 0.0f) {
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
    load_texture_async(&iron_texture, "res/iron.png");
    load_texture_async(&bolt_texture, "res/bolt.png");
    world_id = b2_nullWorldId;
    bodies = NULL;
    n_bodies = 0;
    s_bodies = 0;
}

void physics_tick() {
    if (game_state == SIMULATING) {
        float dt = 1.0f / 30.0f;
        int sub_step_count = 8;
        if (b2World_IsValid(world_id)) {
            b2World_Step(world_id, dt, sub_step_count);
            b2ContactEvents events = b2World_GetContactEvents(world_id);
            for (int e_idx = 0; e_idx < events.hitCount; e_idx++) {
                // I would play a sound here but this is where I was
                // when I decided to throw in the towel
            }
        }
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
        shape_def.material.friction = 0.1f;
        shape_def.material.restitution = 0.1f;
        shape_def.enableContactEvents = true;
        shape_def.enableHitEvents = true;
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

    // Bolt stage
    if (render_bolts != NULL) {
        free(render_bolts);
    }
    render_bolts = malloc(n_bolts * sizeof(PhysicsBolt_t));
    n_render_bolts = 0;
    for (int bolt_idx = 0; bolt_idx < n_bolts; bolt_idx++) {
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

        // Add it to the render list
        b2BodyId first_body = parms.parts[0]->body_id;
        b2Vec2 first_position = b2Body_GetPosition(first_body);
        b2Transform first_transform = {
            { bolt_pos.x - first_position.x, bolt_pos.y - first_position.y },
            { 1.0f, 0.0f },
        };
        render_bolts[n_render_bolts].parent = parms.parts[0];
        render_bolts[n_render_bolts].offset = first_transform.p;
        n_render_bolts++;
        if (parms.count == 1) {
            if (++parms.parts[0]->n_bolts_to_wall >= 2) {
                // 2 bolts makes it static
                b2Body_SetType(parms.parts[0]->body_id, b2_staticBody);
            } else {
                b2RevoluteJointDef joint_def = b2DefaultRevoluteJointDef();
                joint_def.base.bodyIdA = first_body;
                joint_def.base.localFrameA = first_transform;

                // Make a dummy body at the bolt position to bind to
                b2BodyDef wall_anchor_def = b2DefaultBodyDef();
                wall_anchor_def.type = b2_staticBody;
                wall_anchor_def.position = bolt_pos;
                joint_def.base.bodyIdB = b2CreateBody(world_id, &wall_anchor_def);
                b2Transform wall_anchor_transform = {
                    { 0.0f, 0.0f },
                    { 1.0f, 0.0f },
                };
                joint_def.base.localFrameB = wall_anchor_transform;

                b2CreateRevoluteJoint(world_id, &joint_def);
            }
        } else if (parms.count == 2) {
            b2RevoluteJointDef joint_def = b2DefaultRevoluteJointDef();
            joint_def.base.bodyIdA = first_body;
            joint_def.base.localFrameA = first_transform;
            joint_def.base.bodyIdB = parms.parts[1]->body_id;
            b2Vec2 second_position = b2Body_GetPosition(parms.parts[1]->body_id);
            b2Transform second_transform = {
                { bolt_pos.x - second_position.x, bolt_pos.y - second_position.y },
                { 1.0f, 0.0f },
            };
            joint_def.base.localFrameB = second_transform;
            b2CreateRevoluteJoint(world_id, &joint_def);
        } else {
            printf("Unsupported number of parts for one bolt: %d\n", parms.count);
        }
        free(parms.parts);
    }
}

void physics_render() {
    SDL_SetRenderDrawColor(renderer, 0x1D, 0x2E, 0x28, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    for (int body_idx = 0; body_idx < n_bodies; body_idx++) {
        b2WorldTransform transform = b2Body_GetTransform(bodies[body_idx].body_id);
        Triangle_t transformed_triangles[bodies[body_idx].n_triangles];
        Triangle_t texcoord_triangles[bodies[body_idx].n_triangles];
        for (int tri_idx = 0; tri_idx < bodies[body_idx].n_triangles; tri_idx++) {
            transformed_triangles[tri_idx].p[0].vec = b2TransformWorldPoint(transform, bodies[body_idx].triangles[tri_idx].p[0].vec);
            transformed_triangles[tri_idx].p[1].vec = b2TransformWorldPoint(transform, bodies[body_idx].triangles[tri_idx].p[1].vec);
            transformed_triangles[tri_idx].p[2].vec = b2TransformWorldPoint(transform, bodies[body_idx].triangles[tri_idx].p[2].vec);
            texcoord_triangles[tri_idx].p[0].vec = b2MulSV(0.01f, bodies[body_idx].triangles[tri_idx].p[0].vec);
            texcoord_triangles[tri_idx].p[1].vec = b2MulSV(0.01f, bodies[body_idx].triangles[tri_idx].p[1].vec);
            texcoord_triangles[tri_idx].p[2].vec = b2MulSV(0.01f, bodies[body_idx].triangles[tri_idx].p[2].vec);
        }
        
        SDL_FColor vertex_color;
        vertex_color.r = 1.0f;
        vertex_color.g = 1.0f;
        vertex_color.b = 1.0f;
        vertex_color.a = SDL_ALPHA_OPAQUE_FLOAT;
        SDL_GetError();
        SDL_SetRenderTextureAddressMode(renderer, SDL_TEXTURE_ADDRESS_WRAP, SDL_TEXTURE_ADDRESS_WRAP);
        SDL_RenderGeometryRaw(renderer, iron_texture.texture,
            &transformed_triangles[0].p[0].x, sizeof(b2Vec2), // positions
            &vertex_color, 0, // colors
            &texcoord_triangles[0].p[0].x, sizeof(b2Vec2), // texcoords
            bodies[body_idx].n_triangles * 3, NULL, 0, 0); // indicates
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    for (int bolt_idx = 0; bolt_idx < n_render_bolts; bolt_idx++) {
        b2WorldTransform transform = b2Body_GetTransform(render_bolts[bolt_idx].parent->body_id);
        b2Vec2 p = b2TransformWorldPoint(transform, render_bolts[bolt_idx].offset);
        SDL_FRect destination = { p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f };
        SDL_FPoint center = { 4.0f, 4.0f };
        float angle = b2Rot_GetAngle(transform.q);
        SDL_RenderTextureRotated(renderer, bolt_texture.texture, NULL, &destination, angle * (180.0f / M_PI), &center, SDL_FLIP_NONE);
    }
    SDL_RenderPresent(renderer);
}

void physics_teardown() {
    b2DestroyWorld(world_id);
}