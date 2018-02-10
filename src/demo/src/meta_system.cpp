#include "meta_system.hpp"

#include "systems/nim_system.hpp"

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/loading_system.hpp>


namespace mirrage {

	Meta_system::Meta_system(Game_engine& engine)
	  : _entities(engine.assets(), this)
	  , _renderer(engine.renderer_factory().create_renderer(_entities, *this))
	  , _model_loading(std::make_unique<renderer::Loading_system>(_entities, engine.assets()))
	  , _nims(std::make_unique<systems::Nim_system>(_entities)) {
		_entities.register_component_type<ecs::components::Transform_comp>();
	}

	Meta_system::~Meta_system() {
		_renderer->device().wait_idle();
		_entities.clear();
	}

	void Meta_system::update(util::Time dt) {
		_entities.process_queued_actions();

		_nims->update(dt);
		_model_loading->update(dt);
		_renderer->update(dt);
	}
	void Meta_system::draw() { _renderer->draw(); }

	void Meta_system::shrink_to_fit() { _renderer->shrink_to_fit(); }
} // namespace mirrage
