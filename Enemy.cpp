#include "Enemy.hpp"
#include "Scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace glm;

Guard::Guard(Scene *s, Scene::Object *obj) {
    scene = s;
    object = obj;
    object->transform->position = vec3(0.f,0.f,0.75f);

    light = s->new_lamp(s->new_transform());
    light->type = Scene::Lamp::Spot;

    move_dir = UP;
}

Guard::~Guard() {
}

void Guard::update(float elapsed, GameMode *gm) {
    light->transform->rotation = quat_from_dir(move_dir);
    object->transform->position += 0.7f * vec3_from_dir(move_dir) * elapsed;
    light->transform->position = object->transform->position - vec3_from_dir(move_dir)*0.17f;

    vec3 infront = object->transform->position + vec3_from_dir(move_dir) * 0.5f;
    int x = int(round(infront.x));
    int y = int(round(infront.y));

    float posx = object->transform->position.x;
    float posy = object->transform->position.y;
    float difx = abs(posx - round(posx));
    float dify = abs(posy - round(posy));
    turned -= elapsed;
    if(x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_WIDTH || gm->walls[x][y]) {
        if(move_dir == UP || move_dir == DOWN){
            Direction moves[] = {RIGHT, LEFT};
            move_dir = moves[gm->random_gen() % 2];
        } else {
            Direction moves[] = {UP, DOWN};
            move_dir = moves[gm->random_gen() % 2];
        }
    } else if (turned <= 0.f && difx < 0.7f * elapsed && dify < 0.7f * elapsed && gm->random_gen() % 2 == 1) {
        Direction target_dir;
        if(move_dir == UP || move_dir == DOWN){
            Direction moves[] = {RIGHT, LEFT};
            target_dir = moves[gm->random_gen() % 2];
        } else {
            Direction moves[] = {UP, DOWN};
            target_dir = moves[gm->random_gen() % 2];
        }

        infront = object->transform->position + vec3_from_dir(target_dir) * 0.5f;
        x = int(round(infront.x));
        y = int(round(infront.y));
        if(x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_WIDTH && !gm->walls[x][y]) {
            move_dir = target_dir;
        }
        turned = 0.5f;
    }
    //printf("%f, %f\n", object->transform->position.x, object->transform->position.y);
}
bool Guard::can_see_player(GameMode *gm) {

    vec3 pp = gm->player->transform->position;
    vec2 pp2 = vec2(pp.x, pp.y);

    vec3 mp = object->transform->position;
    vec2 mp2 = vec2(mp.x, mp.y);
    vec2 point = pp2 - mp2;

    // Distance of vector between player and guard
    float dist = length(point);
    float angle = acos(dot(point, vec2_from_dir(move_dir)) / dist);
    if (angle < radians(22.5f) && dist < 2.5f) {
        // Need to do some raycasts
        auto is_in_wall = [gm](vec2 pos) {
            int x = int(round(pos.x));
            int y = int(round(pos.y));
            return x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_WIDTH || gm->walls[x][y];
        };

        for(float i = 0.f; i < 1.f; i += 0.1f) {
            if(is_in_wall(mp2 + point * i)) {
                return false;
            }
        }

        return true;
    }
    return false;
}


///// SCOUT //////
Scout::Scout(Scene *s, Scene::Object *obj) {
    scene = s;
    object = obj;
    object->transform->position = vec3(0.f,0.f,3.f);

    light = s->new_lamp(s->new_transform());
    light->type = Scene::Lamp::Spot;
    light->transform->rotation = angleAxis(radians(0.f), vec3(1.f, 0.f, 0.f));
    light->fov = glm::radians(60.0f);

    move_dir = UP;
}

Scout::~Scout() {
}

void Scout::update(float elapsed, GameMode *gm) {
    object->transform->position += 0.5f * vec3_from_dir(move_dir) * elapsed;
    move_time -= elapsed;
    if (object->transform->position.x < 1.f) {
        Direction moves[] = {RIGHT, UP, DOWN};
        move_dir = moves[gm->random_gen() % 3];
        move_time = gm->random_gen() % 5 + 5;
    } else if (object->transform->position.y < 1.f) {
        Direction moves[] = {RIGHT, UP, LEFT};
        move_dir = moves[gm->random_gen() % 3];
        move_time = gm->random_gen() % 5 + 5;
    } else if (object->transform->position.x > MAP_WIDTH - 1.f) {
        Direction moves[] = {LEFT, UP, DOWN};
        move_dir = moves[gm->random_gen() % 3];
        move_time = gm->random_gen() % 5 + 5;
    } else if (object->transform->position.x > MAP_HEIGHT - 1.f) {
        Direction moves[] = {RIGHT, LEFT, DOWN};
        move_dir = moves[gm->random_gen() % 3];
        move_time = gm->random_gen() % 5 + 5;
    } else if (move_time <= 0.0f) {
        Direction moves[] = {RIGHT, UP, DOWN, LEFT};
        move_dir = moves[gm->random_gen() % 4];
        move_time = gm->random_gen() % 5 + 5;
    }

    //printf("%f, %f\n", object->transform->position.x, object->transform->position.y);
    light->transform->position = object->transform->position - vec3(0.f,0.f,0.5f);
}
bool Scout::can_see_player(GameMode *gm) {
    vec3 pp = gm->player->transform->position;
    vec2 pp2 = vec2(pp.x, pp.y);

    vec3 mp = object->transform->position;
    vec2 mp2 = vec2(mp.x, mp.y);
    vec2 dist = pp2 - mp2;
    return dot(dist, dist) < 1.0f;
}