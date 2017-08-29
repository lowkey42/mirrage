/** initialization & event handling of input devices *************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/input/events.hpp>
#include <mirrage/input/types.hpp>

#include <mirrage/utils/messagebus.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <SDL2/SDL.h>
#include <glm/vec2.hpp>
#include <gsl/gsl>
#include <memory>
#include <unordered_map>


namespace mirrage::asset {
	class Asset_manager;
}

namespace mirrage::input {

	class Input_mapper;
	class Input_manager;

	struct Sdl_event_filter {
		Sdl_event_filter(Input_manager&);
		Sdl_event_filter(Sdl_event_filter&&) noexcept;
		Sdl_event_filter& operator=(Sdl_event_filter&&) noexcept;
		virtual ~Sdl_event_filter();
		virtual bool propagate(SDL_Event&) = 0;
		virtual void pre_input_events() {}
		virtual void post_input_events() {}

	  private:
		Input_manager* _manager;
	};

	class Input_manager {
	  private:
		static constexpr auto _max_pointers = 2;

	  public:
		Input_manager(util::Message_bus& bus, asset::Asset_manager&);
		Input_manager(const Input_manager&) = delete;
		Input_manager(Input_manager&&)      = delete;
		~Input_manager() noexcept;

		void update(util::Time dt);

		void add_event_filter(Sdl_event_filter&);
		void remove_event_filter(Sdl_event_filter&);

		void screen_to_world_coords(std::function<glm::vec2(glm::vec2)> func) {
			_screen_to_world_coords = func;
		}
		void viewport(glm::vec4 v) { _viewport = v; }
		void window(SDL_Window* w) { _sdl_window = w; }

		auto last_pointer_world_position(int idx = 0) const noexcept {
			return _pointer_world_pos[gsl::narrow<std::size_t>(idx)];
		}
		auto last_pointer_screen_position(int idx = 0) const noexcept {
			return _pointer_screen_pos[gsl::narrow<std::size_t>(idx)];
		}

		auto pointer_world_position(int idx = 0) const noexcept {
			auto uidx = gsl::narrow<std::size_t>(idx);
			return _pointer_active[uidx] ? util::justCopy(_pointer_world_pos[uidx]) : util::nothing;
		}
		auto pointer_screen_position(int idx = 0) const noexcept {
			auto uidx = gsl::narrow<std::size_t>(idx);
			return _pointer_active[uidx] ? util::justCopy(_pointer_screen_pos[uidx]) : util::nothing;
		}

		void enable_context(Context_id id);

		void world_space_events(bool e) { _world_space_events = e; }

		void capture_mouse(bool enable);
		auto capture_mouse() const noexcept { return _mouse_captured; }

	  private:
		void _add_gamepad(int joystick_id);
		void _remove_gamepad(int instance_id);

		void _on_mouse_motion(const SDL_MouseMotionEvent& motion);

		void _poll_events();
		void _handle_event(SDL_Event& event);

	  private:
		class Gamepad;

		util::Mailbox_collection _mailbox;

		glm::vec4                             _viewport;
		SDL_Window*                           _sdl_window;
		bool                                  _world_space_events = true;
		bool                                  _mouse_captured     = false;
		glm::vec2                             _mouse_capture_screen_pos;
		std::function<glm::vec2(glm::vec2)>   _screen_to_world_coords;
		std::vector<std::unique_ptr<Gamepad>> _gamepads;

		std::vector<Sdl_event_filter*> _event_filter;

		std::array<glm::vec2, _max_pointers> _pointer_screen_pos{};
		std::array<glm::vec2, _max_pointers> _pointer_world_pos{};
		std::array<bool, _max_pointers>      _pointer_active{};
		std::array<int64_t, _max_pointers>   _pointer_finger_id{};

		std::unique_ptr<Input_mapper> _mapper;
	};
}
