#include <mirrage/renderer/loading_system.hpp>

namespace mirrage::renderer {

	Loading_system::Loading_system(ecs::Entity_manager& ecs, asset::Asset_manager& assets)
	  : _ecs(ecs)
	  , _loaded_models(ecs.list<Model_comp>())
	  , _unloaded_models(ecs.list<Model_unloaded_comp>())
	  , _loading_models(ecs.list<Model_loading_comp>())
	  , _assets(assets)
	{
	}

	void Loading_system::update(util::Time time)
	{
		_update(time);

		_finish_loading();
	}
	void Loading_system::_finish_loading()
	{
		for(auto& model : _loading_models) {
			if(model.model().ready()) {
				model.owner(_ecs).process([&](auto& owner) {
					owner.template emplace<Model_comp>(model.model(), model.local_transform());
					owner.template erase<Model_loading_comp>();
				});
			}
		}
	}

	void Loading_system::_load(Model_unloaded_comp& model)
	{
		model.owner(_ecs).process([&](auto& owner) {
			owner.template emplace<Model_loading_comp>(_assets.load<Model>(model.model_aid()),
			                                           model.local_transform());
			owner.template erase<Model_unloaded_comp>();
		});
	}

	void Loading_system::_unload(Model_comp& model)
	{
		model.owner(_ecs).process([&](auto& owner) {
			owner.template emplace<Model_unloaded_comp>(model.model_aid(), model.local_transform());
			owner.template erase<Model_comp>();
		});
	}

	void Loading_system::_update(util::Time)
	{
		for(auto&& model : _unloaded_models) {
			_load(model);
		}
	}
} // namespace mirrage::renderer
