#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("shabby.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("shabby.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > project_whatev(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Project-whatev.wav"));
});

Load< Sound::Sample > wrong_ball(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("wrong_ball.wav"));
});

Load< Sound::Sample > there_you_go(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("there_you_go.wav"));
});

PlayMode::PlayMode() : scene(*hexapod_scene) {
	std::mt19937 mt;
	if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
	collectibles.resize(8);
	play_which.resize(8);
	shrink.resize(8);
	auto rand = mt();
	for (int i=0; i < 8; i++) {
		play_which[i] = rand >> i & 1;
		shrink[i] = -1.0f;
	}
	
	for (auto &transform : scene.transforms) {
		if (transform.name == "Cube") cube = &transform;
		else if (transform.name == "Knot") knot = &transform;
		else if (transform.name == "Collectible1") collectibles[0] = &transform;
		else if (transform.name == "Collectible2") collectibles[1] = &transform;
		else if (transform.name == "Collectible3") collectibles[2] = &transform;
		else if (transform.name == "Collectible4") collectibles[3] = &transform;
		else if (transform.name == "Collectible5") collectibles[4] = &transform;
		else if (transform.name == "Collectible6") collectibles[5] = &transform;
		else if (transform.name == "Collectible7") collectibles[6] = &transform;
		else if (transform.name == "Collectible8") collectibles[7] = &transform;
	}

	cube_base_rotation = cube->rotation;
	knot_base_rotation = knot->rotation;
	cube_base_position = cube->position;
	knot_base_position = knot->position;
	collectible_base_scale = collectibles[0]->scale;
	cube->scale = glm::vec3(0.7f, 0.7f, 0.7f);

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	
	camera = &scene.cameras.front();
	camera_base_position = camera->transform->position;
	camera_base_rotation = camera->transform->rotation;
	camera_rel_cube = camera_base_position - cube_base_position;

	//start music loop playing:
	// (note: position will be over-ridden in update())
	bgm = Sound::loop_3D(*project_whatev, 0.2f, cube_base_position, 10.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			front.downs += 1;
			front.pressed = true;
			if (balloon_blown_up || won) {
				reset = true;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_l) {
			listening = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_p) {
			pinching = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_c) {
			collecting = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_SPACE) {
			front.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
	  	if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			camera_rotation_angle += -evt.motion.xrel;
			angle = camera_rotation_angle / float(window_size.x) * float(M_PI);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (points == 8) {
		won = true;
		bgm->stop(2.0f);
	}
	for (int i=0; i < 8; i++) {
		if (shrink[i] > 0.0) {
			shrink[i] = shrink[i] - 0.03f;
			if (shrink[i] < 0.0f) {
				shrink[i] = 0.0f;
			}
			collectibles[i]->scale = glm::vec3(shrink[i]);
		}
	}

	if (reset && (balloon_blown_up || won)) {
		reset = false;
		balloon_blown_up = false;
		won = false;
		cube->position = cube_base_position;
		knot->position = knot_base_position;
		cube->scale = glm::vec3(0.7f, 0.7f, 0.7f);
		knot->scale = glm::vec3(1.0f, 1.0f, 1.0f);
		camera->transform->position = camera_base_position;
		camera->transform->rotation = camera_base_rotation;
		listening = false;
		pinching = false;
		collecting = false;
		camera_rotation_angle = 0.0f;
		points = 0;
		std::mt19937 mt;
		auto rand = mt();
		for (int i=0; i < 8; i++) {
			play_which[i] = rand >> i & 1;
			shrink[i] = -1.0f;
			collectibles[i]->scale = collectible_base_scale;
		}
		bgm = Sound::loop_3D(*project_whatev, 0.2f, cube_base_position, 10.0f);
	}

	//move camera:

	swing += elapsed / 5.0f;
	swing -= std::floor(swing);

	cube->rotation = cube_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(swing * 2.0f * float(M_PI))),
		glm::vec3(1.0f, 0.0f, 0.0f)
	);

	float knot_swing = swing;

	// reposition cube
	if (!balloon_blown_up && !won) {
		float speed = 0.007f;
		glm::vec3 dir = cube->position - camera->transform->position;
		dir.z = 0.0f;
		if (up.pressed && !down.pressed) {
			cube->position.z += 0.01f;
			knot->position.z += 0.01f;
			camera->transform->position.z += 0.01f;
		}
		if (!up.pressed && down.pressed) {
			cube->position.z -= 0.01f;
			knot->position.z -= 0.01f;
			camera->transform->position.z -= 0.01f;
		} 
		if (front.pressed) {
			cube->position += speed * dir;
			knot->position += speed * dir;
			camera->transform->position += speed * dir;
		}

		cube->position.x = std::min(6.7f, cube->position.x);
		cube->position.x = std::max(-6.7f, cube->position.x);
		cube->position.y = std::min(6.7f, cube->position.y);
		cube->position.y = std::max(-6.7f, cube->position.y);
		cube->position.z = std::max(-0.5f, cube->position.z);
		cube->position.z = std::min(5.5f, cube->position.z);
		
		knot->position.x = std::min(6.7f, knot->position.x);
		knot->position.x = std::max(-6.7f, knot->position.x);
		knot->position.y = std::min(6.7f, knot->position.y);
		knot->position.y = std::max(-6.7f, knot->position.y);
		knot->position.z = std::max(-0.5f, knot->position.z);
		knot->position.z = std::min(5.5f, knot->position.z);

		glm::vec3 tmp = glm::vec3(
			camera_rel_cube.x * cos(angle) - camera_rel_cube.y * sin(angle),
			camera_rel_cube.x * sin(angle) + camera_rel_cube.y * cos(angle),
			camera_rel_cube.z
		);
		camera->transform->position = cube->position + tmp;
		camera->transform->rotation = glm::normalize(
			camera_base_rotation
			* glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f))
		);
	}

	if (listening) {
		for (int i=0; i < 8; i++) {
			float dx = cube->position.x - collectibles[i]->position.x;
			float dy = cube->position.y - collectibles[i]->position.y;
			float dz = cube->position.z - collectibles[i]->position.z;
			if (dx * dx + dy * dy + dz * dz <= 2.0f && shrink[i] == -1.0f) {
				if (play_which[i]) {
					Sound::play_3D(*there_you_go, 0.2f, collectibles[i]->position, 10.0f);
				} else {
					Sound::play_3D(*wrong_ball, 1.0f, collectibles[i]->position, 10.0f);
				}
				break;
			}
		}
		listening = false;
	}

	if (pinching && !collecting) {
		for (int i=0; i < 8; i++) {
			float dx = cube->position.x - collectibles[i]->position.x;
			float dy = cube->position.y - collectibles[i]->position.y;
			float dz = cube->position.z - collectibles[i]->position.z;
			if (dx * dx + dy * dy + dz * dz <= 2.0f && shrink[i] == -1.0f) {
				if (shrink[i] == 0.0f) {
					continue;
				}
				if (play_which[i]) {
					// error; boom
					bgm->stop(2.0f);
					balloon_blown_up = true;
					cube->scale = glm::vec3(100.0f);
				} else {
					// shrink
					if (shrink[i] != 0.0f) {
						shrink[i] = 1.5f;
						points += 1;
						bgm->set_volume(std::min(0.2f * (points + 1), 0.6f), 1.0f);
					}
				}
				break;
			}
		}
		pinching = false;
	}

	if (collecting && !pinching) {
		for (int i=0; i < 8; i++) {
			float dx = cube->position.x - collectibles[i]->position.x;
			float dy = cube->position.y - collectibles[i]->position.y;
			float dz = cube->position.z - collectibles[i]->position.z;
			if (dx * dx + dy * dy + dz * dz <= 2.0f && shrink[i] == -1.0f) {
				if (play_which[i]) {
					// error; boom
					if (shrink[i] != 0.0f) {
						shrink[i] = 1.5f;
						points += 1;
						bgm->set_volume(std::min(0.2f * (points + 1), 0.6f), 1.0f);
					}
				} else {
					// shrink
					bgm->stop(2.0f);
					balloon_blown_up = true;
					cube->scale = glm::vec3(100.0f);
				}
				break;
			}
		}
		collecting = false;
	}

	knot->rotation = knot_base_rotation * glm::angleAxis(
		glm::radians(25.0f * std::sin(knot_swing * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//reset button press counters:
	front.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	GL_ERRORS();
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		if (balloon_blown_up) {
			lines.draw_text("BOOOOOOOOOOOM!!!!!!!!!", glm::vec3(-aspect + 10.0f * H, -1.0 + 16.0f * H, 0.0f),
				glm::vec3(H * 3.0f, 0.0f, 0.0f), glm::vec3(0.0f, H * 3.0f, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Press Space to Restart", glm::vec3(-aspect + 16.0f * H, -1.0 + 8.0f * H, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		} else if (won) {
			lines.draw_text("YAYYYYYYYYYYYY!!!!!!!!!", glm::vec3(-aspect + 10.0f * H, -1.0 + 16.0f * H, 0.0f),
				glm::vec3(H * 3.0f, 0.0f, 0.0f), glm::vec3(0.0f, H * 3.0f, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Press Space to Restart", glm::vec3(-aspect + 16.0f * H, -1.0 + 8.0f * H, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		} else {
			lines.draw_text("Use WS and SPACE to navigate the balloon; adjust camera using the mouse",
				glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		}
	}
}

// glm::vec3 PlayMode::get_leg_tip_position() {
// 	//the vertex position here was read from the model in blender:
// 	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
// }
