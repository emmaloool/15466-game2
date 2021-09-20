#include "TartMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>

GLuint tart_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > tart_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("tart.pnct"));
	tart_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > tart_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("tart.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = tart_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = tart_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

TartMode::TartMode() : scene(*tart_scene) {
	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	// Get pointers to tart shell transforms for convenience
	for (auto &transform : scene.transforms) {
		if (transform.name == "TartBase") {
			tart.base = &transform;
		}
		else if (transform.name == "TartShell") {
			tart.rim = &transform;
		}
		else if (transform.name == "Fruit") {		// TODO change to Cherry after adding remaining fruits
			// Save fruit state
			Fruit fruit;
			fruit.type = Cherry;
			fruit.available = true;
			fruit.staged = false;
			fruit.ready = false;
			fruit.transform = &transform;
			fruit.init_position = transform.position;
			fruit.dest_position = glm::vec3(0);
			fruit.rot_axis = glm::vec3(1.0f, 0.0f, 0.0f); // default x axis
			fruits.push_back(fruit);
			seen_fruits[Cherry] = true;
			std::cout << glm::to_string(fruit.init_position) << std::endl;
		}
		// TODO: add more cases for the other fruits
	}

	if (tart.base == nullptr) throw std::runtime_error("Tart shell base not found.");
	if (tart.rim == nullptr) throw std::runtime_error("Tart shell rim not found.");
	if (tart.base == nullptr) throw std::runtime_error("Tart shell rim not found.");
	if (seen_fruits[Cherry] == false) throw std::runtime_error("Cherry not found.");
	// TODO add more pointer checks for other fruits

	// Initialize "floor" level
	tart_base_depth = tart.base->position.z;

	// "Hide" all loaded fruits
	for (auto &fruit : fruits) {
		fruit.transform->position = tart.base->position;
		fruit.transform->position.z = tart_base_depth - 10.0f;
	}
}

TartMode::~TartMode() {
}

bool TartMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	Fruit &current_fruit = fruits[current_fruit_index];

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_SPACE) {
			// Load current fruit
			Fruit &current_fruit = fruits[current_fruit_index];
			if (current_fruit.available) {
				current_fruit.staged = true;
				current_fruit.transform->position = current_fruit.init_position;
			}
		} 
		
		// TODO move incremental adjustments to updates + base math on downs
		else if (evt.key.keysym.sym == SDLK_LEFT) {
			if (current_fruit.staged) {
				current_fruit.transform->rotation *= glm::angleAxis(
																		- glm::radians(5.0f),
																		current_fruit.rot_axis
																	);
			}
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			if (current_fruit.staged) {
				current_fruit.transform->rotation *=  glm::angleAxis(
																		glm::radians(5.0f),
																		current_fruit.rot_axis
																	);
			}
		} else if (evt.key.keysym.sym == SDLK_x) {
			current_fruit.rot_axis.x = 1.0f;
			current_fruit.rot_axis.y = 0;
			current_fruit.rot_axis.z = 0;
		} else if (evt.key.keysym.sym == SDLK_y) {
			current_fruit.rot_axis.x = 0;
			current_fruit.rot_axis.y = 1.0f;
			current_fruit.rot_axis.z = 0;
		} else if (evt.key.keysym.sym == SDLK_z) {
			current_fruit.rot_axis.x = 0;
			current_fruit.rot_axis.y = 0;
			current_fruit.rot_axis.z = 1.0f;
		} 
		
		// else if (evt.key.keysym.sym == SDLK_a) {
		// 	left.downs += 1;
		// 	left.pressed = true;
		// 	return true;
		// } else if (evt.key.keysym.sym == SDLK_d) {
		// 	right.downs += 1;
		// 	right.pressed = true;
		// 	return true;
		// }
	} 
	// else if (evt.type == SDL_KEYUP) {
	// 	if (evt.key.keysym.sym == SDLK_a) {
	// 		left.pressed = false;
	// 		return true;
	// 	} else if (evt.key.keysym.sym == SDLK_d) {
	// 		right.pressed = false;
	// 		return true;
	// 	}
	// }
	else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		// Calculate and set the final position after clicking on the scene
		// This will be used to test when the fruit intersects the scene after throwing it
		if (current_fruit.staged) {
			assert(current_fruit.available);		// only available fruits can be staged!

			// Set position to initial (loading) position
			current_fruit.transform->position = current_fruit.init_position;

			// Determine the ray to the tart base
			
			// This code is based on the code and guide from this website: 
			// This implementation of determinine the final destination position of the fruit
			// is inspired by Alyssa's code here: https://github.com/lassyla/game2/blob/master/FishMode.cpp
			// This also utilizes game0 PlayMode code to get the mouse-to-clip-space position,
			// but here we want to get a 4-vector for clip coordinates
			glm::vec4 ray_clip = glm::vec4(				// Homogenous clip coordinates
				(evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
				(evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f,
				1.0f,
				1.0f
			);
			glm::vec4 ray_camera = glm::inverse(camera->make_projection()) * ray_clip;
			std::cout << "ray camera: " << glm::to_string(ray_camera);
			ray_camera.z = -1.0f;		// manually setting z, w 
			ray_camera.w = 0.0f;

			glm::vec3 ray_world = glm::vec3(glm::mat4(camera->transform->make_local_to_world()) * ray_camera);
			ray_world = glm::normalize(ray_world);	// Normalize

			float time = (tart_base_depth - current_fruit.transform->position.z) / ray_world.z; // time at which fruit hits plane
			glm::vec3 dest = (ray_world * time) + current_fruit.transform->position;
			dest.z = tart_base_depth; // Fruits intersect tart plane at the level of its base (z-axis)
			// std::cout << "current fruit dest before: " << glm::to_string(current_fruit.dest_position);
			current_fruit.dest_position = dest;
			// std::cout << ", after: " << glm::to_string(current_fruit.dest_position) << std::endl;

			current_fruit.ready = true;
		}
	}

	return false;
}

int8_t TartMode::get_next_available_index() {
	std::cout << "current index: " << unsigned(current_fruit_index) << std::endl;

	if (num_fruit == max_fruit) return -1;

	// Try starting at next modulo index
	uint8_t next_temp = (current_fruit_index + 1) % fruits.size(); //(current_fruit_index + 1) % max_fruit;
	std::cout << "next temp: " << unsigned(next_temp) << std::endl;
	while (next_temp != current_fruit_index) {
		assert(next_temp < fruits.size());
		if (fruits[next_temp].available) {
			return next_temp;
		}
		next_temp = (next_temp + 1) % max_fruit;
	}
	return -1;
}

void TartMode::update(float elapsed) {
	Fruit &current_fruit = fruits[current_fruit_index];

	// Once a fruit gets marked as ready, it's in the process of being thrown
	if (current_fruit.ready) {
		// Collision is tested by checking magnitude of penetration against collision delta
		// If collision occurs, delete the fruit from the scene + update current fruit
		if (glm::length(current_fruit.transform->position - current_fruit.dest_position) < collision_delta) {
			// std::vector<Fruit>::iterator fr = fruits.begin() + current_fruit_index; // TODO sussy math?
			// fruits.erase(current_fruit);

			// Reset current fruit 
			// IMPORTANT: New current fruits start off as available, unstaged, and unready
			fruits[current_fruit_index].available = true;
			fruits[current_fruit_index].staged = false;
			fruits[current_fruit_index].ready = false;

			num_fruit++;	// update number of placed fruits

			int8_t next_index = get_next_available_index();
			if (next_index >= 0) {
				current_fruit_index = (uint8_t)next_index;
				assert(current_fruit_index < fruits.size());
				current_fruit.type 			= fruits[current_fruit_index].type;
				current_fruit.transform 	= fruits[current_fruit_index].transform;
				current_fruit.init_position = fruits[current_fruit_index].init_position;
				current_fruit.dest_position = fruits[current_fruit_index].dest_position;
				current_fruit.rot_axis 		= fruits[current_fruit_index].rot_axis;
			}

			
		}
		else {	// no collision, apply timestep movement 
			current_fruit.transform->position += speed * elapsed * glm::normalize(current_fruit.dest_position - current_fruit.transform->position);
		}

		return;
	}

	//reset button press counters
	{
		left.downs = 0;
		right.downs = 0;
		up.downs = 0;
		down.downs = 0;
	}
}

void TartMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	// { //use DrawLines to overlay some text:
	// 	glDisable(GL_DEPTH_TEST);
	// 	float aspect = float(drawable_size.x) / float(drawable_size.y);
	// 	DrawLines lines(glm::mat4(
	// 		1.0f / aspect, 0.0f, 0.0f, 0.0f,
	// 		0.0f, 1.0f, 0.0f, 0.0f,
	// 		0.0f, 0.0f, 1.0f, 0.0f,
	// 		0.0f, 0.0f, 0.0f, 1.0f
	// 	));

	// 	constexpr float H = 0.09f;
	// 	lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
	// 		glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
	// 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
	// 		glm::u8vec4(0x00, 0x00, 0x00, 0x00));
	// 	float ofs = 2.0f / drawable_size.y;
	// 	lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
	// 		glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
	// 		glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
	// 		glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	// }
}
