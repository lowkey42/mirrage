#pragma once

#include "game_engine.hpp"

#include <mirrage/ecs/ecs.hpp>


namespace lux {
	namespace renderer {class Deferred_renderer;}

	class Meta_system {
		public:
			Meta_system(Game_engine&);
			~Meta_system();

			void update(util::Time dt);
			void draw();

			void shrink_to_fit();

			auto entities()noexcept -> auto& {return _entities;}
			auto renderer()noexcept -> auto& {return *_renderer;}

		private:
			ecs::Entity_manager                          _entities;
			std::unique_ptr<renderer::Deferred_renderer> _renderer;
	};

}
