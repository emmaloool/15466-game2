#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include "GL.hpp"
#include <vector>
#include <array>
#include <deque>

struct TartMode : Mode {
	TartMode();
	virtual ~TartMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;	

	typedef enum FruitKind {
		Cherry = 0,
		Kiwi = 1,
		Peach = 2,
		Blueberry = 3
	} FruitType;

	// Wrapper for instances of various fruit objects
	struct Fruit {
		FruitType type = Cherry;		// Fruits are initialized as cherries
		bool available = true; 			// Fruits will be marked as unavailable when placed on tart
		bool staged = false;			// Only throw fruits that have been properly setup
		bool ready = false;			// Only throw fruits that are ready!
		Scene::Transform *transform;	// Transform associated with this fruit
		glm::vec3 init_position;		// Useful for preparing fruit/throwing fruit
		glm::vec3 dest_position;		// Final position after throwing fruit onto scene
		glm::quat rotation;				// Rotate fruit before placing on tart
	};
	std::vector<Fruit> fruits;
	std::array<bool, 4> seen_fruits; // just to make sure all fruits were loaded

	// Fruit &current_fruit;
	uint8_t current_fruit_index = 0;
	int8_t max_fruit = 10;		// also the max score
	int8_t num_fruit = 0;		// number of fruits placed
	int8_t get_next_available_index();

	// Tart Shell
	struct Tart {
		Scene::Transform *base;
		Scene::Transform *rim;
	};
	Tart tart;
	float tart_base_depth = 1.0f;

	// Collisions/throwing constants
	const float collision_delta = 1.5f;
	const float speed = 25.0f;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
