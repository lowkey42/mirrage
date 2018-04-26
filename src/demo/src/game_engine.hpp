/** An engine containing game-specific manager instances *********************
 *                                                                           *
 * Copyright (c) 2017 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/engine.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/utils/maybe.hpp>


namespace mirrage {
	namespace renderer {
		class Deferred_renderer_factory;
	}

	class Game_engine : public Engine {
	  public:
		Game_engine(const std::string& org,
		            const std::string& title,
		            std::uint32_t      version_major,
		            std::uint32_t      version_minor,
		            bool               debug,
		            int                argc,
		            char**             argv,
		            char**             env);
		~Game_engine();

		auto renderer_factory() noexcept -> auto& { return *_renderer_factory; }

	  protected:
		void _on_post_frame(util::Time) override;

	  private:
		std::unique_ptr<renderer::Deferred_renderer_factory> _renderer_factory;
	};
} // namespace mirrage
