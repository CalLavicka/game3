#pragma once

#include "Scene.hpp"
#include "GameMode.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace glm;

struct Enemy {
public:
    virtual ~Enemy() {}
    virtual void update(float elapsed, GameMode *gm) { }
    virtual bool can_see_player(GameMode *gm) {return false;}


    Scene::Object *object;
    Scene::Lamp *light;
    Scene *scene;
};

enum Direction : char {
    UP = 'u',
    DOWN = 'd',
    LEFT = 'l',
    RIGHT = 'r'
};

static inline Direction direction_left(Direction d) {
    switch(d) {
    case UP:
        return LEFT;
    case LEFT:
        return DOWN;
    case DOWN:
        return RIGHT;
    case RIGHT:
        return UP;
    }
}

static inline Direction direction_right(Direction d) {
    switch(d) {
    case UP:
        return RIGHT;
    case RIGHT:
        return DOWN;
    case DOWN:
        return LEFT;
    case LEFT:
        return UP;
    }
}

static inline uvec2 move_space(uvec2 input, Direction dir) {
    switch(dir) {
    case UP:
        return uvec2(input.x, input.y + 1);
    case DOWN:
        return uvec2(input.x, input.y - 1);
    case LEFT:
        return uvec2(input.x - 1, input.y);
    case RIGHT:
        return uvec2(input.x + 1, input.y);
    }
}

static quat quat_1 = angleAxis(radians(-90.f), vec3(0.f,1.f,0.f));
static quat quat_2 = angleAxis(radians(90.f), vec3(1.f,0.f,0.f));
static quat quat_3 = angleAxis(radians(90.f), vec3(0.f,1.f,0.f));
static quat quat_4 = angleAxis(radians(-90.f), vec3(1.f,0.f,0.f));
static inline quat quat_from_dir(Direction d) {
    switch(d) {
    case UP:
        return quat_1;
    case RIGHT:
        return quat_2;
    case DOWN:
        return quat_3;
    case LEFT:
        return quat_4;
    }
}

static inline vec3 vec3_from_dir(Direction d) {
    switch(d) {
    case UP:
        return vec3(1,0,0);
    case RIGHT:
        return vec3(0,1,0);
    case DOWN:
        return vec3(-1,0,0);
    case LEFT:
        return vec3(0,-1,0);
    }
}

static inline vec2 vec2_from_dir(Direction d) {
    switch(d) {
    case UP:
        return vec2(1,0);
    case RIGHT:
        return vec2(0,1);
    case DOWN:
        return vec2(-1,0);
    case LEFT:
        return vec2(0,-1);
    }
}

struct Guard : public Enemy {
    Guard(Scene *s, Scene::Object *obj);
    virtual ~Guard();

    virtual void update(float elapsed, GameMode *gm) override;
    virtual bool can_see_player(GameMode *gm) override;
    Direction move_dir;
    float turned = 0.f;
};

struct Scout : public Enemy {
    Scout(Scene *s, Scene::Object *obj);
    virtual ~Scout();

    virtual void update(float elapsed, GameMode *gm) override;
    virtual bool can_see_player(GameMode *gm) override;
    Direction move_dir;
    float move_time = 0.f;
};