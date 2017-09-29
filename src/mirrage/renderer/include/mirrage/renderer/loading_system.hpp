#pragma once

#include <mirrage/renderer/model.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/ecs.hpp>
#include <mirrage/utils/units.hpp>


namespace mirrage::renderer {

	/**
	 * Handles the loading and unloading of models.
	 * Has to be instantiated and updated for ANY model components to be loaded and rendered!
	 *
	 * This implementation always loads all model components as soon as they are added and never
	 *   unloads anything automatically.
	 * Subclasses may implement a different behaviour by overriding _update() and NOT calling
	 *   the base class implementation but _load(...) and _unload(...) themself.
	 */
	class Loading_system {
	  public:
		Loading_system(ecs::Entity_manager& ecs, Model_loader&);
		virtual ~Loading_system() = default;

		void update(util::Time);

	  protected:
		Model_comp::Pool&          _loaded_models;
		Model_unloaded_comp::Pool& _unloaded_models;
		Model_loading_comp::Pool&  _loading_models;

		void _load(Model_unloaded_comp&);
		void _unload(Model_comp&);

		virtual void _update(util::Time);

	  private:
		Model_loader& _loader;

		void _finish_loading();
	};
} // namespace mirrage::renderer
