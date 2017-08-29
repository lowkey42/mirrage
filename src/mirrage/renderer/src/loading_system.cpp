#include <mirrage/renderer/loading_system.hpp>

namespace mirrage::renderer {

	Loading_system::Loading_system(ecs::Entity_manager& ecs, Model_loader& loader)
	  : _loaded_models(ecs.list<Model_comp>())
	  , _unloaded_models(ecs.list<Model_unloaded_comp>())
	  , _loading_models(ecs.list<Model_loading_comp>())
	  , _loader(loader) {}

	void Loading_system::update(util::Time time) {
		_update(time);

		_finish_loading();
	}
	void Loading_system::_finish_loading() {
		for(auto& model : _loading_models) {
			if(is_future_ready(model.model())) {
				auto owner = model.owner();
				owner.emplace<Model_comp>(model.model_aid(), get_future_value(model.model()));
				owner.erase<Model_loading_comp>();
			}
		}
	}

	void Loading_system::_load(Model_unloaded_comp& model) {
		auto owner = model.owner();
		owner.emplace<Model_loading_comp>(model.model_aid(), _loader.load(model.model_aid()));
		owner.erase<Model_unloaded_comp>();
	}

	void Loading_system::_unload(Model_comp& model) {
		auto owner = model.owner();
		owner.emplace<Model_unloaded_comp>(model.model_aid());
		owner.erase<Model_comp>();
	}

	void Loading_system::_update(util::Time) {
		for(auto&& model : _unloaded_models) {
			_load(model);
		}
	}
}
