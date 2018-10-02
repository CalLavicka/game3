#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "check_fb.hpp" //helper for checking currently bound OpenGL framebuffer
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "load_save_png.hpp"
#include "texture_program.hpp"
#include "depth_program.hpp"
#include "Enemy.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <stdio.h>


Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("maze.pnct"));
});

Load< GLuint > meshes_for_texture_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(texture_program->program));
});

Load< GLuint > meshes_for_depth_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(depth_program->program));
});

//used for fullscreen passes:
Load< GLuint > empty_vao(LoadTagDefault, [](){
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindVertexArray(0);
	return new GLuint(vao);
});

Load< GLuint > blur_program(LoadTagDefault, [](){
	GLuint program = compile_program(
		//this draws a triangle that covers the entire screen:
		"#version 330\n"
		"void main() {\n"
		"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
		"}\n"
		,
		//NOTE on reading screen texture:
		//texelFetch() gives direct pixel access with integer coordinates, but accessing out-of-bounds pixel is undefined:
		//	vec4 color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0);
		//texture() requires using [0,1] coordinates, but handles out-of-bounds more gracefully (using wrap settings of underlying texture):
		//	vec4 color = texture(tex, gl_FragCoord.xy / textureSize(tex,0));

		"#version 330\n"
		"uniform sampler2D tex;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec2 at = (gl_FragCoord.xy - 0.5 * textureSize(tex, 0)) / textureSize(tex, 0).y;\n"
		//make blur amount more near the edges and less in the middle:
		"	float amt = (0.01 * textureSize(tex,0).y) * max(0.0,(length(at) - 0.3)/0.2);\n"
		//pick a vector to move in for blur using function inspired by:
		//https://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner
		"	vec2 ofs = amt * normalize(vec2(\n"
		"		fract(dot(gl_FragCoord.xy ,vec2(12.9898,78.233))),\n"
		"		fract(dot(gl_FragCoord.xy ,vec2(96.3869,-27.5796)))\n"
		"	));\n"
		"   ofs = vec2(0,0);"
		//do a four-pixel average to blur:
		"	vec4 blur =\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.x,ofs.y)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.y,ofs.x)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.x,-ofs.y)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.y,-ofs.x)) / textureSize(tex, 0))\n"
		"	;\n"
		"	fragColor = vec4(blur.rgb, 1.0);\n" //blur;\n"
		"}\n"
	);

	glUseProgram(program);

	glUniform1i(glGetUniformLocation(program, "tex"), 0);

	glUseProgram(0);

	return new GLuint(program);
});


GLuint load_texture(std::string const &filename) {
	glm::uvec2 size;
	std::vector< glm::u8vec4 > data;
	load_png(filename, &size, &data, LowerLeftOrigin);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	GL_ERRORS();

	return tex;
}

Load< GLuint > wood_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wood.png")));
});

Load< GLuint > marble_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/marble.png")));
});

Load< GLuint > white_tex(LoadTagDefault, [](){
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glm::u8vec4 white(0xff, 0xff, 0xff, 0xff);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, glm::value_ptr(white));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	return new GLuint(tex);
});


Scene::Transform *camera_parent_transform = nullptr;
Scene::Camera *camera = nullptr;
Scene::Transform *spot_parent_transform = nullptr;
Scene::Lamp *spot = nullptr;

void GameMode::new_level() {

	scene.~Scene();
	new(&scene) Scene();

	//pre-build some program info (material) blocks to assign to each object:
	Scene::Object::ProgramInfo texture_program_info;
	texture_program_info.program = texture_program->program;
	texture_program_info.vao = *meshes_for_texture_program;
	texture_program_info.mvp_mat4  = texture_program->object_to_clip_mat4;
	texture_program_info.mv_mat4x3 = texture_program->object_to_light_mat4x3;
	texture_program_info.itmv_mat3 = texture_program->normal_to_light_mat3;

	Scene::Object::ProgramInfo depth_program_info;
	depth_program_info.program = depth_program->program;
	depth_program_info.vao = *meshes_for_depth_program;
	depth_program_info.mvp_mat4  = depth_program->object_to_clip_mat4;
	
	{ // Create the camera
		camera_parent_transform = scene.new_transform();
		camera_parent_transform->position = vec3(0.f, 0.f, 0.f);
		camera_parent_transform->rotation = angleAxis(0.3f, vec3(1.f,0.f,0.f));
		Scene::Transform *cam_trans = scene.new_transform();
		cam_trans->position = vec3(0.f, 0.f, 8.f);
		cam_trans->rotation = angleAxis(0.0f, vec3(1.f,0.f,0.f));
		cam_trans->set_parent(camera_parent_transform);
		camera = scene.new_camera(cam_trans);
	}

	{ // Create the player
		player_pos = scene.new_transform();
		player_pos->rotation = angleAxis(0.f,vec3(1.f,0.f,0.f));
		player = scene.new_object(player_pos);
		player_lamp = scene.new_lamp(scene.new_transform());
		//player_lamp->transform->set_parent(player_pos);
   		player_lamp->type = Scene::Lamp::Spot;
		player_lamp->fov = glm::radians(50.0f);
		player_lamp->transform->position = vec3(0.f,0.f,0.f);

		player_pos->position = vec3(MAP_WIDTH/2, 1.f, 0.75f);
		player_pos->scale = vec3(0.5f,0.5f,0.5f);
		camera_parent_transform->set_parent(player_pos);

		player->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
		player->programs[Scene::Object::ProgramTypeDefault].textures[0] = *marble_tex;
		

		player->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

		MeshBuffer::Mesh const &mesh = meshes->lookup("Cube");
		player->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
		player->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

		player->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
		player->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
	}

	{ // Reset the map
		for(int i = 0; i < MAP_WIDTH; i++) {
			for(int j = 0; j < MAP_HEIGHT; j++) {
				walls[i][j] = true;
			}
		}
	}

	std::vector<uvec3> dead_ends;
	uvec3 longest_end;
	{ // Generate walls and dead ends

		std::random_device r;
		std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
		std::mt19937 rnd{seed};
		random_gen = rnd;

		auto can_place = [this](uvec2 pos) {
			return pos.x != 0 && pos.y != 0 && pos.x < MAP_WIDTH - 1 && pos.y < MAP_HEIGHT - 1 && walls[pos.x][pos.y];
		};

		auto is_valid_move = [this, &can_place](uvec2 pos, Direction dir) {
			
			// Check if can place
			if (!can_place(move_space(pos, dir)))
				return false;
			
			switch(dir) {
			case UP:
				return walls[pos.x - 1][pos.y + 2] && walls[pos.x][pos.y + 2] && walls[pos.x + 1][pos.y + 2]
					&& walls[pos.x - 1][pos.y + 1] && walls[pos.x + 1][pos.y + 1];
			case DOWN:
				return walls[pos.x - 1][pos.y - 2] && walls[pos.x][pos.y - 2] && walls[pos.x + 1][pos.y - 2]
					&& walls[pos.x - 1][pos.y - 1] && walls[pos.x + 1][pos.y - 1];
			case LEFT:
				return walls[pos.x - 2][pos.y + 1] && walls[pos.x - 2][pos.y] && walls[pos.x - 2][pos.y - 1]
					&& walls[pos.x - 1][pos.y + 1] && walls[pos.x - 1][pos.y - 1];
			case RIGHT:
				return walls[pos.x + 2][pos.y + 1] && walls[pos.x + 2][pos.y] && walls[pos.x + 2][pos.y - 1]
					&& walls[pos.x + 1][pos.y + 1] && walls[pos.x + 1][pos.y - 1];
			}
		};

		std::function<void(uvec2, uint)> gen_step;
		gen_step = [this, &gen_step, &dead_ends, &longest_end, &is_valid_move](uvec2 pos, uint length){
			walls[pos.x][pos.y] = false;

			std::vector<Direction> valid = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
			bool dead_end = true;
			while(!valid.empty()) {
				size_t index = random_gen() % valid.size();
				Direction dir = valid[index];
				valid.erase(valid.begin() + index);

				if(is_valid_move(pos, dir)) {
					dead_end = false;
					gen_step(move_space(pos, dir), length + 1);
				}
			}
			if (dead_end) {
				dead_ends.emplace_back(pos, length);
				if(length > longest_end.z) {
					longest_end = uvec3(pos, length);
				}
			}
		};

		gen_step(uvec2(MAP_WIDTH/2, 1), 0);
	}
	
	{ // Add objects for walls
		objects.clear();

		for(int i=0; i<MAP_WIDTH; i++) {
			for(int j=0; j<MAP_HEIGHT; j++) {
				if (walls[i][j]) {
					Scene::Object *obj = scene.new_object(scene.new_transform());
					objects.push_back(obj);
					obj->transform->position = vec3(i,j,1);
					// Scale a little bit to prevent edges
					obj->transform->scale = vec3(1.001f, 1.001f, 1.f);

					obj->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
					obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *wood_tex;
					

					obj->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

					MeshBuffer::Mesh const &mesh = meshes->lookup("Cube");
					obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
					obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

					obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
					obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
				}
			}
		}

		// Add floor
		Scene::Object *obj = scene.new_object(scene.new_transform());
		objects.push_back(obj);
		obj->transform->position = vec3(MAP_WIDTH/2.f - 0.5f, MAP_HEIGHT/2.f - 0.5f, 0.f);
		obj->transform->scale = vec3(MAP_WIDTH, MAP_HEIGHT, 1.f);

		obj->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
		obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *marble_tex;
		

		obj->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

		MeshBuffer::Mesh const &mesh = meshes->lookup("Cube");
		obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
		obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

		obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
		obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
	}

	{ // Create goal
		goal = scene.new_object(scene.new_transform());
		
		goal->transform->position = vec3(longest_end.x, longest_end.y, 0.5f);
		goal->transform->scale = vec3(0.3f, 0.3f, 0.3f);

		goal->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
		goal->programs[Scene::Object::ProgramTypeDefault].textures[0] = *white_tex;
		

		goal->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

		MeshBuffer::Mesh const &mesh = meshes->lookup("Cube");
		goal->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
		goal->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

		goal->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
		goal->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
	}
	

	{ // Create enemies
		for (Enemy *enemy : enemies) {
			delete enemy;
		}
		enemies.clear();
		for(int i=0; i<4; i++) {
			if (dead_ends.empty()) continue;
			Scene::Object *obj = scene.new_object(scene.new_transform());
			obj->transform->scale = vec3(0.3f, 0.3f, 0.3f);

			obj->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
			obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *marble_tex;
			

			obj->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

			MeshBuffer::Mesh const &mesh = meshes->lookup("Cube");
			obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
			obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

			obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
			obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;

			Guard *s = new Guard(&scene, obj);
			size_t index = random_gen() % dead_ends.size();
			uvec3 deadend = dead_ends[index];
			dead_ends.erase(dead_ends.begin() + index);

			obj->transform->position.y = deadend.y;
			obj->transform->position.x = deadend.x;
			enemies.push_back(s);
		}

		for(int i=0; i<4; i++) {
			Scene::Object *obj = scene.new_object(scene.new_transform());
			obj->transform->scale = vec3(0.3f, 0.3f, 0.3f);

			obj->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
			obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *marble_tex;
			

			obj->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

			MeshBuffer::Mesh const &mesh = meshes->lookup("Cube");
			obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
			obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

			obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
			obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;

			Scout *s = new Scout(&scene, obj);
			obj->transform->position.y = 10.f;
			obj->transform->position.x = i*2.f + 5.f;
			enemies.push_back(s);
		}
	}
}

GameMode::GameMode() {

	new_level();
}

GameMode::~GameMode() {
}

static float mousex;
static float mousey;

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (evt.type == SDL_MOUSEMOTION) {
		if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			camera_spin += 5.0f * evt.motion.xrel / float(window_size.x);
			return true;
		}
		if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
			spot_spin += 5.0f * evt.motion.xrel / float(window_size.x);
			return true;
		}

		mousex = evt.motion.x / float(window_size.x) - 0.5f;
		mousey = evt.motion.y / float(window_size.y) - 0.5f;

		player_lamp->transform->rotation = glm::angleAxis(glm::atan(mousex, mousey), glm::vec3(0.f,0.f,1.f))
									* glm::angleAxis(glm::radians(-90.f), glm::vec3(1.f,0.f,0.f));

    	//player_lamp->transform->rotation = angleAxis(radians(0.f), vec3(1.f, 0.f, 0.f));
		//printf("X: %f, Y: %f\n", mousex, mousey);

		return true;

	} else if (evt.type == SDL_KEYDOWN) {
		switch(evt.key.keysym.scancode) {
		case SDL_SCANCODE_D:
			controls.right = true;
			break;
		case SDL_SCANCODE_A:
			controls.left = true;
			break;
		case SDL_SCANCODE_W:
			controls.up = true;
			break;
		case SDL_SCANCODE_S:
			controls.down = true;
			break;
		default:
			return false;
		}

		return true;
	} else if (evt.type == SDL_KEYUP) {
		switch(evt.key.keysym.scancode) {
		case SDL_SCANCODE_D:
			controls.right = false;
			break;
		case SDL_SCANCODE_A:
			controls.left = false;
			break;
		case SDL_SCANCODE_W:
			controls.up = false;
			break;
		case SDL_SCANCODE_S:
			controls.down = false;
			break;
		default:
			return false;
		}
	}

	return false;
}

void GameMode::update(float elapsed) {
	//camera_parent_transform->rotation = glm::angleAxis(camera_spin, glm::vec3(0.0f, 0.0f, 1.0f));
	//spot_parent_transform->rotation = glm::angleAxis(spot_spin, glm::vec3(0.0f, 0.0f, 1.0f));
	if(!dead) {
		auto is_in_wall = [this](vec3 pos) {
			auto test = [this](vec3 testp) {
				int x = int(round(testp.x));
				int y = int(round(testp.y));
				return x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_WIDTH || walls[x][y];
			};

			return test(pos + vec3(0.25f,0.25f,0.f)) || test(pos + vec3(0.25f,-0.25f,0.f))
				|| test(pos + vec3(-0.25f,0.25f,0.f)) || test(pos + vec3(-0.25f,-0.25f,0.f));
		};

		static float speed = 3.f;
		if (controls.right) {
			player_pos->position.x += speed * elapsed;
			if(is_in_wall(player_pos->position))
				player_pos->position.x -= speed * elapsed;
		}
		if (controls.left) {
			player_pos->position.x -= speed * elapsed;
			if(is_in_wall(player_pos->position))
				player_pos->position.x += speed * elapsed;
		}
		if (controls.up) {
			player_pos->position.y += speed * elapsed;
			if(is_in_wall(player_pos->position))
				player_pos->position.y -= speed * elapsed;
		}
		if (controls.down) {
			player_pos->position.y -= speed * elapsed;
			if(is_in_wall(player_pos->position))
				player_pos->position.y += speed * elapsed;
		}
		player_lamp->transform->position = player_pos->position;

		bool seen = false;
		for(Enemy *enemy : enemies) {
			if(enemy->can_see_player(this)) {
				seen = true;
			}
			enemy->update(elapsed, this);
		}
		static float seen_timer = 0.3f;
		if(seen) {
			seen_timer -= elapsed;
			if(seen_timer <= 0.f) {
				show_end_screen("YOU LOSE");
			}
		} else {
			seen_timer = min(seen_timer + elapsed, 0.3f);
		}

		{ // See if player hits goal
			vec2 pp = vec2(player->transform->position.x, player->transform->position.y);
			vec2 gp = vec2(goal->transform->position.x, goal->transform->position.y);
			if(dot(pp-gp, pp-gp) < 0.4f) {
				show_end_screen("YOU WIN");
			}
		}
		//printf("NEXT\n");
	}
}

//GameMode will render to some offscreen framebuffer(s).
//This code allocates and resizes them as needed:
struct Framebuffers {
	glm::uvec2 size = glm::uvec2(0,0); //remember the size of the framebuffer

	//This framebuffer is used for fullscreen effects:
	GLuint color_tex = 0;
	GLuint depth_rb = 0;
	GLuint fb = 0;

	//This framebuffer is used for shadow maps:
	glm::uvec2 shadow_size = glm::uvec2(0,0);
	GLuint shadow_color_tex = 0; //DEBUG
	GLuint shadow_depth_tex = 0;
	GLuint shadow_fb = 0;

	void allocate(glm::uvec2 const &new_size, glm::uvec2 const &new_shadow_size) {
		//allocate full-screen framebuffer:
		if (size != new_size) {
			size = new_size;

			if (color_tex == 0) glGenTextures(1, &color_tex);
			glBindTexture(GL_TEXTURE_2D, color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (depth_rb == 0) glGenRenderbuffers(1, &depth_rb);
			glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
	
			if (fb == 0) glGenFramebuffers(1, &fb);
			glBindFramebuffer(GL_FRAMEBUFFER, fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}

		//allocate shadow map framebuffer:
		if (shadow_size != new_shadow_size) {
			shadow_size = new_shadow_size;

			if (shadow_color_tex == 0) glGenTextures(1, &shadow_color_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, shadow_size.x, shadow_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);


			if (shadow_depth_tex == 0) glGenTextures(1, &shadow_depth_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_size.x, shadow_size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (shadow_fb == 0) glGenFramebuffers(1, &shadow_fb);
			glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadow_color_tex, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_depth_tex, 0);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}
	}
} fbs;

void GameMode::draw(glm::uvec2 const &drawable_size) {
	fbs.allocate(drawable_size, glm::uvec2(512, 512));

	glBindFramebuffer(GL_FRAMEBUFFER, fbs.fb);
	glViewport(0,0,drawable_size.x, drawable_size.y);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Draw once for ambient light
	glUseProgram(texture_program->program);

	//don't use distant directional light at all (color == 0):
	glUniform3fv(texture_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(texture_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
	//little bit of ambient light:
	glUniform3fv(texture_program->sky_color_vec3, 1, glm::value_ptr(dead ? glm::vec3(0.5f, 0.5f, 0.5f) : glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(texture_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));
	
	glUniform3fv(texture_program->spot_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	scene.draw(camera);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GL_ERRORS();

	auto render_from_spot = [&drawable_size, this](Scene::Lamp *spot) {
		//printf("LAMP: %f %f %f\n", spot->transform->position.x, spot->transform->position.y, spot->transform->position.z);
		//Draw scene to shadow map for spotlight:
		glBindFramebuffer(GL_FRAMEBUFFER, fbs.shadow_fb);
		glViewport(0,0,fbs.shadow_size.x, fbs.shadow_size.y);

		glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		//render only back faces to shadow map (prevent shadow speckles on fronts of objects):
		glCullFace(GL_FRONT);
		glEnable(GL_CULL_FACE);

		scene.draw(spot, Scene::Object::ProgramTypeShadow);

		glDisable(GL_CULL_FACE);

		glBindFramebuffer(GL_FRAMEBUFFER, fbs.fb);
		glViewport(0,0,drawable_size.x, drawable_size.y);

		GL_ERRORS();



		//Draw scene to off-screen framebuffer:
		glBindFramebuffer(GL_FRAMEBUFFER, fbs.fb);

		camera->aspect = drawable_size.x / float(drawable_size.y);


		//set up basic OpenGL state:
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc (GL_SRC_ALPHA, GL_DST_ALPHA);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glDepthFunc(GL_LEQUAL);
		//glClear(GL_DEPTH_BUFFER_BIT);

		//set up light positions:
		glUseProgram(texture_program->program);

		//don't use distant directional light at all (color == 0):
		glUniform3fv(texture_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
		glUniform3fv(texture_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
		//no ambient light:
		glUniform3fv(texture_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
		glUniform3fv(texture_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

		glm::mat4 world_to_spot =
			//This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture coordinates ([0,1]^2) and depth map Z values ([0,1]):
			glm::mat4(
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.5f, 0.0f,
				0.5f, 0.5f, 0.5f+0.00001f /* <-- bias */, 1.0f
			)
			//this is the world-to-clip matrix used when rendering the shadow map:
			* spot->make_projection() * spot->transform->make_world_to_local();

		glUniformMatrix4fv(texture_program->light_to_spot_mat4, 1, GL_FALSE, glm::value_ptr(world_to_spot));

		glm::mat4 spot_to_world = spot->transform->make_local_to_world();
		glUniform3fv(texture_program->spot_position_vec3, 1, glm::value_ptr(glm::vec3(spot_to_world[3])));
		glUniform3fv(texture_program->spot_direction_vec3, 1, glm::value_ptr(-glm::vec3(spot_to_world[2])));
		glUniform3fv(texture_program->spot_color_vec3, 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.f)));

		glm::vec2 spot_outer_inner = glm::vec2(std::cos(0.5f * spot->fov), std::cos(0.85f * 0.5f * spot->fov));
		glUniform2fv(texture_program->spot_outer_inner_vec2, 1, glm::value_ptr(spot_outer_inner));

		//This code binds texture index 1 to the shadow map:
		// (note that this is a bit brittle -- it depends on none of the objects in the scene having a texture of index 1 set in their material data; otherwise scene::draw would unbind this texture):
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, fbs.shadow_depth_tex);
		//The shadow_depth_tex must have these parameters set to be used as a sampler2DShadow in the shader:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
		//NOTE: however, these are parameters of the texture object, not the binding point, so there is no need to set them *each frame*. I'm doing it here so that you are likely to see that they are being set.
		glActiveTexture(GL_TEXTURE0);

		scene.draw(camera);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		GL_ERRORS();
	};

	for(Enemy *enemy : enemies) {
		vec3 dif_vec = enemy->object->transform->position - player->transform->position;
		// Don't render lights outside of viewport
		if(abs(dif_vec.x) > 7.f || abs(dif_vec.y) > 6.f) {
			continue;
		}

		//printf("ENEMY %p\n", enemy);
		GLuint old_count = enemy->object->programs[Scene::Object::ProgramTypeShadow].count;
		enemy->object->programs[Scene::Object::ProgramTypeShadow].count = 0;
		render_from_spot(enemy->light);
		enemy->object->programs[Scene::Object::ProgramTypeShadow].count = old_count;
	}

	// Don't render player in light
	GLuint old_count = player->programs[Scene::Object::ProgramTypeShadow].count;
	player->programs[Scene::Object::ProgramTypeShadow].count = 0;
	render_from_spot(player_lamp);
	player->programs[Scene::Object::ProgramTypeShadow].count = old_count;
	
	//Copy scene from color buffer to screen, performing post-processing effects:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbs.color_tex);
	glUseProgram(*blur_program);
	glBindVertexArray(*empty_vao);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}


void GameMode::show_end_screen(std::string message) {

	controls.left = false;
	controls.right = false;
	controls.up = false;
	controls.down = false;

	dead = true;

	std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();

	std::shared_ptr< Mode > game = shared_from_this();
	menu->background = game;
	menu->background_fade = 0.5f;

	menu->choices.emplace_back(message);
	menu->choices.emplace_back("TRY AGAIN", [game, this](){
		Mode::set_current(game);
		dead = false;
		this->new_level();
	});
	menu->choices.emplace_back("QUIT", [](){
		Mode::set_current(nullptr);
	});

	menu->selected = 1;

	Mode::set_current(menu);
}