/** maps inputs to actions ***************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/input/types.hpp>
#include <mirrage/input/events.hpp>

#include <mirrage/utils/units.hpp>
#include <mirrage/utils/messagebus.hpp>
#include <mirrage/utils/str_id.hpp>

#include <glm/vec2.hpp>
#include <SDL2/SDL.h>
#include <memory>
#include <unordered_map>


namespace lux {
	namespace asset {
		class Asset_manager;
	}

namespace input {

	struct Reaction {
		Action_id     action;
		Reaction_type type = Reaction_type::none;
	};

	struct Context {
		Context_id id;
		std::unordered_map<Key, Reaction>          keys;
		std::unordered_map<Pad_button, Reaction>   pad_buttons;
		std::unordered_map<Pad_stick, Reaction>    pad_sticks;
		std::unordered_map<Mouse_click, Reaction>  mouse_buttons;
		Reaction                                   mouse_movement;
		Reaction                                   mouse_wheel_up;
		Reaction                                   mouse_wheel_down;
		Reaction                                   mouse_drag;
	};


	class Input_mapper {
		public:
			Input_mapper(util::Message_bus& bus, asset::Asset_manager& assets);

			void enable_context(Context_id id);

			void on_key_pressed (Key);
			void on_key_released(Key);

			void on_mouse_pos_change(glm::vec2 rel, glm::vec2 abs);

			void on_mouse_wheel_change(glm::vec2 rel);

			void on_pad_button_pressed (Input_source src, Pad_button,
			                            float intensity);
			void on_pad_button_changed (Input_source src, Pad_button,
			                            float intensity);
			void on_pad_button_released(Input_source src, Pad_button);

			void on_mouse_button_pressed (Mouse_button, float pressure=1.f);
			void on_mouse_button_released(Mouse_button, int8_t clicks);

			void on_pad_stick_change(Input_source src, Pad_stick, glm::vec2 rel, glm::vec2 abs);

		private:
			util::Message_bus& _bus;

			std::unordered_map<Context_id, Context> _context;
			Context_id _default_context_id;
			Context_id _active_context_id;
			Context* _active_context;

			bool _primary_mouse_button_down = false;
			bool _is_mouse_drag = false;
	};


	struct Mapped_inputs {
		std::vector<Key>          keys;
		std::vector<Pad_button>   pad_buttons;
		std::vector<Pad_stick>    sticks;
		std::vector<Mouse_click>  mouse_buttons;
		bool mouse_movement   = false;
		bool mouse_wheel_up   = false;
		bool mouse_wheel_down = false;
		bool mouse_drag       = false;
	};

	class Input_mapping_updater {
		public:
			Input_mapping_updater(asset::Asset_manager& assets);

			void set_context(Context_id id);

			auto get(Action_id)const -> const Mapped_inputs&;

			void add(Action_id action, Key k);
			void add(Action_id action, Pad_button b);
			void add(Action_id action, Pad_stick s);
			void add(Action_id action, Mouse_button m, int8_t clicks);
			void add_mwheel(Action_id action, bool up);
			void add_move(Action_id action);
			void add_drag(Action_id action);

			void clear(Action_id action);

			void save();

		private:
			auto reaction_type_for(Action_id)const -> Reaction_type;

			static const Mapped_inputs no_mapped_input;

			asset::Asset_manager& _assets;

			std::unordered_map<Context_id, Context> _context;
			std::unordered_map<Action_id, Mapped_inputs> _cached_actions;
			std::unordered_map<Action_id, Reaction_type> _cached_reaction_types;
			Context_id _default_context_id;
			Context_id _active_context_id;
			Context* _active_context;
	};

}
}
