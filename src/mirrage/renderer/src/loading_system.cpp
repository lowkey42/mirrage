#include <mirrage/renderer/loading_system.hpp>

namespace lux {
namespace renderer {

	Loading_system::Loading_system(ecs::Entity_manager& ecs,
	                               Model_loader& loader)
	    : _loaded_models(ecs.list<Model_comp>())
	    , _unloaded_models(ecs.list<Model_unloaded_comp>())
	    , _loading_models(ecs.list<Model_loading_comp>())
	    , _loader(loader) {
	}

	void Loading_system::update(util::Time time) {
		_update(time);

		_finish_loading();
	}
	void Loading_system::_finish_loading() {
		for(auto& model : _loading_models) {
			// check if load has finished and replace ourself with the real model_comp
			if(model.model().wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
				auto owner = model.owner();
				owner.emplace<Model_comp>(model.model_aid(), model.model().get());
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
}
