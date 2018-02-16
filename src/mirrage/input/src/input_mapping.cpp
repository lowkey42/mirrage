#include <sf2/sf2.hpp>

#include <mirrage/input/input_mapping.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <glm/gtx/norm.hpp>

namespace mirrage::input {

	sf2_structDef(Reaction, action, type);
	sf2_structDef(Context,
	              id,
	              keys,
	              pad_buttons,
	              pad_sticks,
	              mouse_buttons,
	              mouse_movement,
	              mouse_wheel_up,
	              mouse_wheel_down,
	              mouse_drag);

	namespace {
		const auto      mapping_aid             = "cfg:input_mapping"_aid;
		constexpr float min_mouse_drag_movement = 1.f;

		struct Context_map {
			Context_id                                       initial;
			std::unordered_map<Context_id, Context>          contexts;
			mutable std::unordered_map<Context_id, Context>* live_contexts = nullptr;

			Context_map() = default;
			Context_map(Context_id initial, std::unordered_map<Context_id, Context> contexts)
			  : initial(std::move(initial)), contexts(std::move(contexts)) {}

			Context_map(Context_map&& rhs) noexcept
			  : initial(std::move(rhs.initial)), contexts(std::move(rhs.contexts)) {}

			Context_map& operator=(Context_map&& rhs) noexcept {
				initial  = std::move(rhs.initial);
				contexts = std::move(rhs.contexts);
				if(live_contexts)
					*live_contexts = contexts;

				return *this;
			}
		};

		sf2_structDef(Context_map, initial, contexts);

		std::tuple<std::unordered_map<Context_id, Context>, Context_id> load_context_map(
		        asset::Asset_manager& assets, std::unordered_map<Context_id, Context>& map) {
			auto cm = assets.load<Context_map>(mapping_aid);

			cm->live_contexts = &map;

			return std::make_tuple(cm->contexts, cm->initial);
		}

		void save_context_map(asset::Asset_manager&                          assets,
		                      const std::unordered_map<Context_id, Context>& map,
		                      Context_id                                     def) {

			assets.save(mapping_aid, Context_map{def, map});
		}

		template <class Container>
		auto find_maybe(Container& c, typename Container::key_type k) {
			auto iter = c.find(k);
			return iter != c.end() ? util::justPtr(&iter->second) : util::nothing;
		}
	} // namespace

	Input_mapper::Input_mapper(util::Message_bus& bus, asset::Asset_manager& assets) : _bus(bus) {

		std::tie(_context, _default_context_id) = load_context_map(assets, _context);
		enable_context(_default_context_id);
	}

	void Input_mapper::enable_context(Context_id id) {
		_active_context_id = id;

		auto def_ctx = _context.find(_active_context_id);
		MIRRAGE_INVARIANT(def_ctx != _context.end(), "Enabled undefined input context");
		_active_context = &def_ctx->second;
	}

	namespace {

		void process_pressed(util::Message_bus& bus,
		                     const Reaction&    action,
		                     Input_source       src       = 0,
		                     float              intensity = 1.f) {
			switch(action.type) {
				case Reaction_type::continuous: bus.send<Continuous_action>(action.action, src, true); break;

				case Reaction_type::range:
					bus.send<Range_action>(action.action, src, glm::vec2{intensity, 0}, glm::vec2{0, 0});
					break;

				case Reaction_type::once:
				default: break;
			}
		}

		void process_release(util::Message_bus& bus, const Reaction& action, Input_source src = 0) {
			switch(action.type) {
				case Reaction_type::continuous: bus.send<Continuous_action>(action.action, src, false); break;

				case Reaction_type::range:
					bus.send<Range_action>(action.action, src, glm::vec2{0, 0}, glm::vec2{0, 0});
					break;

				case Reaction_type::once: bus.send<Once_action>(action.action, src); break;
				default: break;
			}
		}

		void process_movement(util::Message_bus& bus,
		                      const Reaction&    action,
		                      Input_source       src,
		                      glm::vec2          rel,
		                      glm::vec2          abs) {
			switch(action.type) {
				case Reaction_type::continuous:
					bus.send<Continuous_action>(action.action, src, true);
					bus.send<Continuous_action>(action.action, src, false);
					break;

				case Reaction_type::range: bus.send<Range_action>(action.action, src, rel, abs); break;

				case Reaction_type::once: bus.send<Once_action>(action.action, src); break;

				default: break;
			}
		}
	} // namespace

	void Input_mapper::on_key_pressed(Key k) {
		find_maybe(_active_context->keys, k).process([&](auto& action) { process_pressed(_bus, action); });
	}

	void Input_mapper::on_key_released(Key k) {
		find_maybe(_active_context->keys, k).process([&](auto& action) { process_release(_bus, action); });
	}

	void Input_mapper::on_mouse_pos_change(glm::vec2 rel, glm::vec2 abs) {
		process_movement(_bus, _active_context->mouse_movement, Input_source(0), rel, abs);

		if(_primary_mouse_button_down && (_is_mouse_drag || glm::length2(rel) >= min_mouse_drag_movement)) {
			_is_mouse_drag = true;
			process_movement(_bus, _active_context->mouse_drag, Input_source(0), rel, abs);
		}
	}

	void Input_mapper::on_mouse_wheel_change(glm::vec2 rel) {
		auto& action = rel.y > 0 ? _active_context->mouse_wheel_up : _active_context->mouse_wheel_down;

		switch(action.type) {
			case Reaction_type::range:
				_bus.send<Range_action>(action.action, Input_source(0), rel, rel);
				break;

			case Reaction_type::once: _bus.send<Once_action>(action.action, Input_source(0)); break;

			case Reaction_type::continuous:
				_bus.send<Continuous_action>(action.action, Input_source(0), true);
				_bus.send<Continuous_action>(action.action, Input_source(0), false);
			default: break;
		}
	}

	void Input_mapper::on_pad_button_pressed(Input_source src, Pad_button b, float intensity) {
		find_maybe(_active_context->pad_buttons, b).process([&](auto& action) {
			process_pressed(_bus, action, src, intensity);
		});
	}

	void Input_mapper::on_pad_button_changed(Input_source src, Pad_button b, float intensity) {
		find_maybe(_active_context->pad_buttons, b).process([&](auto& action) {
			if(action.type == Reaction_type::range)
				_bus.send<Range_action>(action.action, src, glm::vec2{intensity, 0}, glm::vec2{intensity, 0});
		});
	}

	void Input_mapper::on_pad_button_released(Input_source src, Pad_button b) {
		find_maybe(_active_context->pad_buttons, b).process([&](auto& action) {
			process_release(_bus, action, src);
		});
	}

	void Input_mapper::on_mouse_button_pressed(Mouse_button b, float pressure) {
		find_maybe(_active_context->mouse_buttons, {b, 0}).process([&](auto& action) {
			process_pressed(_bus, action, Input_source{0}, pressure);
		});

		if(b == 1)
			_primary_mouse_button_down = true;
	}

	void Input_mapper::on_mouse_button_released(Mouse_button b, int8_t clicks) {
		if(b != 1 || !_is_mouse_drag) {
			find_maybe(_active_context->mouse_buttons, {b, clicks}).process([&](auto& action) {
				process_release(_bus, action);
			});

			// -1 = any
			find_maybe(_active_context->mouse_buttons, {b, -1}).process([&](auto& action) {
				process_release(_bus, action);
			});
		}

		// call end listerners for continue_click_handlers (clicks==0)
		find_maybe(_active_context->mouse_buttons, {b, 0}).process([&](auto& action) {
			process_release(_bus, action);
		});

		if(b == 1) {
			_primary_mouse_button_down = false;
			_is_mouse_drag             = false;
		}
	}

	void Input_mapper::on_pad_stick_change(Input_source src, Pad_stick s, glm::vec2 rel, glm::vec2 abs) {
		find_maybe(_active_context->pad_sticks, s).process([&](auto& action) {
			process_movement(_bus, action, src, rel, abs);
		});
	}


	const Mapped_inputs Input_mapping_updater::no_mapped_input = {};

	Input_mapping_updater::Input_mapping_updater(asset::Asset_manager& assets) : _assets(assets) {

		std::tie(_context, _default_context_id) = load_context_map(assets, _context);
		set_context(_default_context_id);
	}

	void Input_mapping_updater::set_context(Context_id id) {
		_active_context_id = id;

		auto def_ctx = _context.find(_active_context_id);
		MIRRAGE_INVARIANT(def_ctx != _context.end(), "Enabled undefined input context");
		_active_context = &def_ctx->second;

		_cached_reaction_types.clear();
		_cached_actions.clear();

		// fill _cached_actions
		for(const auto& r : _active_context->keys) {
			_cached_actions[r.second.action].keys.push_back(r.first);
			_cached_reaction_types.emplace(r.second.action, r.second.type);
		}

		for(const auto& r : _active_context->pad_buttons) {
			_cached_actions[r.second.action].pad_buttons.push_back(r.first);
			_cached_reaction_types.emplace(r.second.action, r.second.type);
		}

		for(const auto& r : _active_context->pad_sticks) {
			_cached_actions[r.second.action].sticks.push_back(r.first);
			_cached_reaction_types.emplace(r.second.action, r.second.type);
		}

		for(const auto& r : _active_context->mouse_buttons) {
			_cached_actions[r.second.action].mouse_buttons.push_back(r.first);
			_cached_reaction_types.emplace(r.second.action, r.second.type);
		}

		_cached_actions[_active_context->mouse_wheel_up.action].mouse_wheel_up     = true;
		_cached_actions[_active_context->mouse_wheel_down.action].mouse_wheel_down = true;

		_cached_reaction_types.emplace(_active_context->mouse_wheel_up.action,
		                               _active_context->mouse_wheel_up.type);

		_cached_reaction_types.emplace(_active_context->mouse_wheel_down.action,
		                               _active_context->mouse_wheel_down.type);
	}

	auto Input_mapping_updater::get(Action_id id) const -> const Mapped_inputs& {
		auto iter = _cached_actions.find(id);
		return iter != _cached_actions.end() ? iter->second : no_mapped_input;
	}

	auto Input_mapping_updater::reaction_type_for(Action_id a) const -> Reaction_type {
		auto type = _cached_reaction_types.find(a);
		MIRRAGE_INVARIANT(type != _cached_reaction_types.end(), "Unknown action");

		return type->second;
	}

	void Input_mapping_updater::add(Action_id action, Key k) {
		auto type = reaction_type_for(action);

		auto& mapped_action = _active_context->keys[k];

		if(mapped_action.type != Reaction_type::none)
			util::erase_fast(_cached_actions[mapped_action.action].keys, k);

		mapped_action = {action, type};
		_cached_actions[action].keys.push_back(k);
	}

	void Input_mapping_updater::add(Action_id action, Pad_button b) {
		auto type = reaction_type_for(action);

		auto& mapped_action = _active_context->pad_buttons[b];

		if(mapped_action.type != Reaction_type::none)
			util::erase_fast(_cached_actions[mapped_action.action].pad_buttons, b);

		mapped_action = {action, type};
		_cached_actions[action].pad_buttons.push_back(b);
	}

	void Input_mapping_updater::add(Action_id action, Pad_stick s) {
		auto type = reaction_type_for(action);

		auto& mapped_action = _active_context->pad_sticks[s];

		if(mapped_action.type != Reaction_type::none)
			util::erase_fast(_cached_actions[mapped_action.action].sticks, s);

		mapped_action = {action, type};
		_cached_actions[action].sticks.push_back(s);
	}

	void Input_mapping_updater::add(Action_id action, Mouse_button m, int8_t clicks) {
		auto type = reaction_type_for(action);

		auto click = Mouse_click{m, clicks};

		auto& mapped_action = _active_context->mouse_buttons[click];

		if(mapped_action.type != Reaction_type::none)
			util::erase_fast(_cached_actions[mapped_action.action].mouse_buttons, click);

		mapped_action = {action, type};
		_cached_actions[action].mouse_buttons.push_back(click);
	}
	void Input_mapping_updater::add_mwheel(Action_id action, bool up) {
		auto type = reaction_type_for(action);

		if(up) {
			auto old_action                            = _active_context->mouse_wheel_up.action;
			_cached_actions[old_action].mouse_wheel_up = false;

			_active_context->mouse_wheel_up        = {action, type};
			_cached_actions[action].mouse_wheel_up = true;

		} else {
			auto old_action                            = _active_context->mouse_wheel_up.action;
			_cached_actions[old_action].mouse_wheel_up = false;

			_active_context->mouse_wheel_up        = {action, type};
			_cached_actions[action].mouse_wheel_up = true;
		}
	}

	void Input_mapping_updater::add_move(Action_id action) {
		auto type = reaction_type_for(action);

		auto old_action                            = _active_context->mouse_movement.action;
		_cached_actions[old_action].mouse_movement = false;

		_active_context->mouse_movement        = {action, type};
		_cached_actions[action].mouse_movement = true;
	}
	void Input_mapping_updater::add_drag(Action_id action) {
		auto type = reaction_type_for(action);

		auto old_action                        = _active_context->mouse_drag.action;
		_cached_actions[old_action].mouse_drag = false;

		_active_context->mouse_drag        = {action, type};
		_cached_actions[action].mouse_drag = true;
	}

	void Input_mapping_updater::clear(Action_id action) {
		auto& inputs = _cached_actions[action];

		for(const auto& k : inputs.keys)
			_active_context->keys.erase(k);

		for(const auto& b : inputs.pad_buttons)
			_active_context->pad_buttons.erase(b);

		for(const auto& s : inputs.sticks)
			_active_context->pad_sticks.erase(s);

		for(const auto& b : inputs.mouse_buttons)
			_active_context->mouse_buttons.erase(b);

		if(inputs.mouse_movement)
			_active_context->mouse_movement = Reaction();

		if(inputs.mouse_wheel_up)
			_active_context->mouse_wheel_up = Reaction();

		if(inputs.mouse_wheel_down)
			_active_context->mouse_wheel_down = Reaction();

		if(inputs.mouse_drag)
			_active_context->mouse_drag = Reaction();

		inputs = Mapped_inputs{};
	}

	void Input_mapping_updater::save() { save_context_map(_assets, _context, _default_context_id); }
} // namespace mirrage::input
