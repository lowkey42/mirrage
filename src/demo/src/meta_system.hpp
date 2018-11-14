#pragma once

#include "game_engine.hpp"

#include <mirrage/ecs/ecs.hpp>
#include <mirrage/gui/debug_ui.hpp>


namespace mirrage {
	namespace renderer {
		class Deferred_renderer;
	}
	namespace renderer {
		class Loading_system;
	}
	namespace systems {
		class Nim_system;
	}


	class Meta_system {
	  public:
		Meta_system(Game_engine&);
		~Meta_system();

		void update(util::Time dt);
		void draw();

		void shrink_to_fit();

		auto entities() noexcept -> auto& { return _entities; }
		auto renderer() noexcept -> auto& { return *_renderer; }
		auto nims() noexcept -> auto& { return *_nims; }

	  private:
		ecs::Entity_manager                          _entities;
		std::unique_ptr<renderer::Deferred_renderer> _renderer;
		std::unique_ptr<renderer::Loading_system>    _model_loading;
		std::unique_ptr<systems::Nim_system>         _nims;
		util::Console_command_container              _commands;
	};
} // namespace mirrage
