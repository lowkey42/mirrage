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
#include <mirrage/gui/debug_ui.hpp>
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
		~Game_engine() override;

		auto renderer_factory() noexcept -> auto& { return *_renderer_factory; }
		auto global_render() noexcept -> auto& { return *_global_render; }
		auto render_pass_mask() noexcept -> auto& { return _render_pass_mask; }

	  protected:
		void _on_post_frame(util::Time) override;

	  private:
		gui::Debug_ui                                        _debug_ui;
		std::unique_ptr<renderer::Deferred_renderer_factory> _renderer_factory;
		std::unique_ptr<renderer::Deferred_renderer>         _global_render;
		renderer::Render_pass_mask                           _render_pass_mask;
	};
} // namespace mirrage
