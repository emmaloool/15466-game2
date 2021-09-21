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
	// Helper to setup fruit
	auto setup_fruit = [this](FruitType type, Scene::Transform &transform, std::string name) {
		Fruit fruit;
		fruit.name = name;
		fruit.type = type;
		fruit.available = true;
		fruit.staged = false;
		fruit.ready = false;
		fruit.transform = &transform;
		fruit.init_position = transform.position;
		fruit.dest_position = glm::vec3(0);
		fruit.rot_axis = glm::vec3(1.0f, 0.0f, 0.0f); // default x axis
		fruits.push_back(fruit);
		seen_fruits[type] = true;
	};

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
		else if (transform.name == "TartCream") {
			tart.cream = &transform;
		}
		else if (transform.name == "Cherry") {
			setup_fruit(Cherry, transform, "Cherry");
		}
		else if (transform.name == "Blueberry") {
			setup_fruit(Blueberry, transform, "Blueberry");
		}
		else if (transform.name == "Banana") {
			setup_fruit(Banana, transform, "Banana");
		}
		else if (transform.name == "GreenKiwi") {
			setup_fruit(GreenKiwi, transform, "Green Kiwi");
		}
		else if (transform.name == "YellowKiwi") {
			setup_fruit(YellowKiwi, transform, "Yellow Kiwi");
		}
		else if (transform.name == "Honeydew") {
			setup_fruit(Honeydew, transform, "Honeydew");
		}
		else if (transform.name == "Cantaloupe") {
			setup_fruit(Cantaloupe, transform, "Cantaloupe");
		}
		else if (transform.name == "Watermelon") {
			setup_fruit(Watermelon, transform, "Watermelon");
		}
		else if (transform.name == "WhiteDragonFruit") {
			setup_fruit(WhiteDragonFruit, transform, "White Dragon Fruit");
		}
		else if (transform.name == "RedDragonFruit") {
			setup_fruit(RedDragonFruit, transform, "Red Dragon Fruit");
		}
	}

	if (tart.base == nullptr) throw std::runtime_error("Tart shell base not found.");
	if (tart.rim == nullptr) throw std::runtime_error("Tart shell rim not found.");
	if (tart.cream == nullptr) throw std::runtime_error("Tart pastry cream not found.");
	if (tart.base == nullptr) throw std::runtime_error("Tart shell rim not found.");
	if (seen_fruits[Cherry] == false) throw std::runtime_error("Cherry not found.");
	if (seen_fruits[Blueberry] == false) throw std::runtime_error("Blueberry not found.");
	if (seen_fruits[Banana] == false) throw std::runtime_error("Banana not found.");
	if (seen_fruits[GreenKiwi] == false) throw std::runtime_error("GreenKiwi not found.");
	if (seen_fruits[YellowKiwi] == false) throw std::runtime_error("YellowKiwi not found.");
	if (seen_fruits[Honeydew] == false) throw std::runtime_error("Honeydew not found.");
	if (seen_fruits[Cantaloupe] == false) throw std::runtime_error("Cantaloupe not found.");
	if (seen_fruits[Watermelon] == false) throw std::runtime_error("Watermelon not found.");
	if (seen_fruits[WhiteDragonFruit] == false) throw std::runtime_error("WhiteDragonFruit not found.");
	if (seen_fruits[RedDragonFruit] == false) throw std::runtime_error("RedDragonFruit not found.");

	// Initialize "floor" level + hidden position
	tart_base_depth = tart.base->position.z;
	hidden_fruit_pos = glm::vec3(0.0f, 0.0f, tart_base_depth - 10.0f);

	// "Hide" all loaded fruits
	for (auto &fruit : fruits) {
		fruit.transform->position = hidden_fruit_pos;
	}
}

TartMode::~TartMode() {
}

int8_t TartMode::get_next_available_index() {
	uint8_t next_temp = (current_fruit_index + 1) % fruits.size(); // Start at wraparound index
	while (next_temp != current_fruit_index) {
		if (fruits[next_temp].available) {
			current_fruit_index = (uint8_t)next_temp;
			return next_temp;
		}
		next_temp = (next_temp + 1) % max_fruit;
	}
	return -1;
}

bool TartMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	auto load_fruit = [](Fruit &fruit) {
		fruit.available = true;
		fruit.staged = true;
		fruit.transform->position = fruit.init_position;
	};
	auto unload_fruit = [this](Fruit &fruit) {
		fruit.staged = false;
		fruit.transform->position = hidden_fruit_pos;
	};

	Fruit &current_fruit = fruits[current_fruit_index];

	if (evt.type == SDL_KEYDOWN) {
		// Handle fruit loading/throwing
		if (evt.key.keysym.sym == SDLK_SPACE) {
			Fruit &current_fruit = fruits[current_fruit_index];
			if (current_fruit.available) {
				load_fruit(current_fruit);
			}
			return true;
		} 
		else if (evt.key.keysym.sym == SDLK_t) {
			if (current_fruit.staged) {	// Only allow switching fruits while initial fruit has been loaded
				// Unload current fruit
				unload_fruit(current_fruit);

				// Load next available fruit
				// If there are no available fruit other than the current one, the current fruit gets reloaded
				get_next_available_index();
				Fruit &next_fruit = fruits[current_fruit_index];
				load_fruit(next_fruit);
			}
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_u) {
			if (!placed_fruit_indices.empty()) {
				// Unload current fruit (to make way for current one)
				unload_fruit(current_fruit); 

				// Load most previously placed fruit
				uint8_t placed_index = placed_fruit_indices.top();
				placed_fruit_indices.pop();
				current_fruit_index = placed_index;
				Fruit &last_fruit = fruits[placed_index];
				load_fruit(last_fruit);
				num_fruit--;
			}
			return true;
		}

		// Handle rotations (change axis, amount of rotation)
		else if (evt.key.keysym.sym == SDLK_1) {
			if (current_fruit.staged) {
				current_fruit.rot_axis.x = 1.0f;
				current_fruit.rot_axis.y = 0;
				current_fruit.rot_axis.z = 0;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_2) {
			if (current_fruit.staged) {
				current_fruit.rot_axis.x = 0;
				current_fruit.rot_axis.y = 1.0f;
				current_fruit.rot_axis.z = 0;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_3) {
			if (current_fruit.staged) {
				current_fruit.rot_axis.x = 0;
				current_fruit.rot_axis.y = 0;
				current_fruit.rot_axis.z = 1.0f;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			if (current_fruit.staged) {
				current_fruit.transform->rotation *= glm::angleAxis(
																		- glm::radians(5.0f),
																		current_fruit.rot_axis
																	);
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			if (current_fruit.staged) {
				current_fruit.transform->rotation *=  glm::angleAxis(
																		glm::radians(5.0f),
																		current_fruit.rot_axis
																	);
			}
			return true;
		}

		// Handle camera movement
		else if (evt.key.keysym.sym == SDLK_RETURN) {
			if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
				SDL_SetRelativeMouseMode(SDL_TRUE);
				return true;
			}
		}
		else if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;			
		} else if (evt.key.keysym.sym == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}

	} 

	// Handle camera movement some more
	else if (evt.type == SDL_KEYUP) {
		
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.pressed = false;
			return true;
		}
	}
	
	else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		
		if (current_fruit.staged) {
			// assert(current_fruit.available);		// only available fruits can be staged!

			// Calculate and set the final position after clicking on the scene
			// This will be used to test when the fruit intersects the scene after throwing it
			{
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
				ray_camera.z = -1.0f;		// manually setting z, w, to be safe...
				ray_camera.w = 0.0f;

				glm::vec3 ray_world = glm::vec3(glm::mat4(camera->transform->make_local_to_world()) * ray_camera);
				ray_world = glm::normalize(ray_world);	// Normalize

				float time = (tart_base_depth - current_fruit.transform->position.z) / ray_world.z; // time at which fruit hits plane
				glm::vec3 dest = (ray_world * time) + current_fruit.transform->position;
				dest.z = tart_base_depth; // Fruits intersect tart plane at the level of its base (z-axis)

				// After loading fruit, the fruit is ready to be thrown
				current_fruit.dest_position = dest;
			}

			current_fruit.ready = true;
		}
		return true;
	} 
	
	else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}
	
	return false;
}

void TartMode::update(float elapsed) {
	Fruit &current_fruit = fruits[current_fruit_index];

	// Once a fruit gets marked as ready, it's in the process of being thrown
	if (current_fruit.ready) {
		// Collision is tested by checking magnitude of penetration against collision delta
		// If collision occurs, delete the fruit from the scene + update current fruit
		if (glm::length(current_fruit.transform->position - current_fruit.dest_position) < collision_delta) {

			placed_fruit_indices.push(current_fruit_index);		// Add placement to tracker

			// Reset current fruit 
			// IMPORTANT: New current fruits start off as available, unstaged, and unready
			fruits[current_fruit_index].available = false;
			fruits[current_fruit_index].staged = false;
			fruits[current_fruit_index].ready = false;

			num_fruit++;	// Update number of placed fruit
			get_next_available_index();
		}
		else {	// no collision, apply timestep movement 
			current_fruit.transform->position += speed * elapsed * glm::normalize(current_fruit.dest_position - current_fruit.transform->position);
		}
	}

	//move camera:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
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
		if (num_fruit == max_fruit) {
			lines.draw_text("You finished the tart! :)",
				glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text("You finished the tart! :)",
				glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else {
			lines.draw_text("Current Fruit: " + fruits[current_fruit_index].name + ", # Fruits Placed: " + std::to_string(num_fruit),
				glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text("Current Fruit: " + fruits[current_fruit_index].name + ", # Fruits Placed: " + std::to_string(num_fruit),
				glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
}
