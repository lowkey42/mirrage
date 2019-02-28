/** initialization, live-cycle management & glue-code ************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/screen.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/messagebus.hpp>

#include <memory>
#include <vector>


union SDL_Event;

namespace mirrage {
	namespace asset {
		class Asset_manager;
	}
	namespace input {
		class Input_manager;
	}
	namespace graphic {
		class Context;
		class Device;
		class Window;
	} // namespace graphic
	namespace audio {
		class Audio_ctx;
	}
	namespace gui {
		class Gui;
	}
	namespace net {
		class Net_manager;
	}
	class Translator;
	struct Engine_event_filter;

	extern std::string get_sdl_error();

	class Engine {
	  public:
		Engine(const std::string&       org,
		       const std::string&       title,
		       util::maybe<std::string> base_dir,
		       std::uint32_t            version_major,
		       std::uint32_t            version_minor,
		       bool                     debug,
		       bool                     headless,
		       int                      argc,
		       char**                   argv,
		       char**                   env);
		virtual ~Engine() noexcept;

		bool running() const noexcept { return !_quit; }
		void exit() noexcept { _quit = true; }

		void on_frame();

		auto& graphics_context() noexcept { return *_graphics_context; }
		auto& graphics_context() const noexcept { return *_graphics_context; }
		auto& window() noexcept { return _graphics_main_window.get_or_throw("No window created!"); }
		auto& window() const noexcept { return _graphics_main_window.get_or_throw("No window created!"); }
		auto& assets() noexcept { return *_asset_manager; }
		auto& assets() const noexcept { return *_asset_manager; }
		auto& input() noexcept { return *_input_manager; }
		auto& input() const noexcept { return *_input_manager; }
		auto& bus() noexcept { return _bus; }
		auto& screens() noexcept { return _screens; }
		auto& translator() noexcept { return *_translator; }
		auto& net() noexcept { return *_net_manager; }
		auto& gui() noexcept { return *_gui; }

	  protected:
		virtual void _on_pre_frame(util::Time) {}
		virtual void _on_post_frame(util::Time) {}

	  protected:
		struct Sdl_wrapper {
			Sdl_wrapper(bool headless);
			~Sdl_wrapper();
		};

		bool                                  _quit = false;
		Screen_manager                        _screens;
		util::Message_bus                     _bus;
		std::unique_ptr<asset::Asset_manager> _asset_manager;
		std::unique_ptr<Translator>           _translator;
		Sdl_wrapper                           _sdl;
		std::unique_ptr<graphic::Context>     _graphics_context;
		util::maybe<graphic::Window&>         _graphics_main_window;
		std::unique_ptr<input::Input_manager> _input_manager;
		std::unique_ptr<Engine_event_filter>  _event_filter;
		std::unique_ptr<net::Net_manager>     _net_manager;
		std::unique_ptr<gui::Gui>             _gui;

		double _current_time = 0;
		double _last_time    = 0;

		bool _headless;
	};
} // namespace mirrage
