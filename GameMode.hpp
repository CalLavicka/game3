#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Scene.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <random>

// The 'GameMode' mode is the main gameplay mode:

#define MAP_WIDTH 22
#define MAP_HEIGHT 17

struct Enemy;

struct GameMode : public Mode {
	GameMode();
	virtual ~GameMode();

	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

	//update is called at the start of a new frame, after events are handled:
	virtual void update(float elapsed) override;

	//draw is called after update:
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void new_level();
	void show_end_screen(std::string message);

	struct {
		bool up = false;
		bool down = false;
		bool left = false;
		bool right = false;
	} controls;

	Scene scene;

	float camera_spin = 0.0f;
	float spot_spin = 0.0f;

	std::mt19937 random_gen;
	bool walls[MAP_WIDTH][MAP_HEIGHT];
	Scene::Object *player;
	Scene::Transform *player_pos;
	Scene::Lamp *player_lamp;
	bool dead = false;

	Scene::Object *goal;
	
	std::vector<Enemy*> enemies;
	std::vector<Scene::Object *> objects;
};
