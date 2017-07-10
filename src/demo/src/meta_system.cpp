#include "meta_system.hpp"

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>


namespace lux {

	Meta_system::Meta_system(Game_engine& engine)
	    : _entities(engine.assets(), this)
	    , _renderer(engine.renderer_factory().create_renderer(_entities, *this)) {
		_entities.register_component_type<ecs::components::Transform_comp>();
	}

	Meta_system::~Meta_system() {
		_renderer->device().wait_idle();
		_entities.clear();
	}

	void Meta_system::update(util::Time dt) {
		_entities.process_queued_actions();

		_renderer->update(dt);
	}
	void Meta_system::draw() {
		_renderer->draw();
	}

	void Meta_system::shrink_to_fit() {
		_renderer->shrink_to_fit();
	}

}
