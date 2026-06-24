#include "vmm.h"

#include <stdio.h>

#include <box2d/box2d.h>

b2WorldId world_id;
b2BodyId ground_id;
b2BodyId dynamic_id;

void physics_init() {
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = (b2Vec2){ 0.0f, -10.0f };
    world_id = b2CreateWorld(&world_def);

    b2BodyDef ground_body_def = b2DefaultBodyDef();
    ground_body_def.position = (b2Vec2){ 0.0f, -10.0f };
    ground_id = b2CreateBody(world_id, &ground_body_def);
    b2Polygon ground_box = b2MakeBox(50.0f, 10.0f);
    b2ShapeDef ground_shape_def = b2DefaultShapeDef();
    b2CreatePolygonShape(ground_id, &ground_shape_def, &ground_box);

    b2BodyDef dynamic_body_def = b2DefaultBodyDef();
    dynamic_body_def.type = b2_dynamicBody;
    dynamic_body_def.position = (b2Vec2){ 0.0f, 4.0f };
    dynamic_id = b2CreateBody(world_id, &dynamic_body_def);
    b2Polygon dynamic_box = b2MakeBox(1.0f, 1.0f);
    b2ShapeDef dynamic_shape_def = b2DefaultShapeDef();
    dynamic_shape_def.density = 1.0f;
    dynamic_shape_def.material.friction = 0.3f;
    b2CreatePolygonShape(dynamic_id, &dynamic_shape_def, &dynamic_box);
}

void physics_tick() {
    float dt = 1.0f / 60.0f;
    int sub_step_count = 4;
    b2World_Step(world_id, dt, sub_step_count);
    b2Vec2 position = b2Body_GetPosition(dynamic_id);
}

void physics_teardown() {
    b2DestroyWorld(world_id);
}