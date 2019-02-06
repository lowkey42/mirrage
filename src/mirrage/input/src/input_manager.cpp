#include <mirrage/input/input_manager.hpp>

#include <mirrage/input/input_mapping.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/ranges.hpp>

#include <glm/glm.hpp>
#include <sf2/sf2.hpp>

#include <SDL_gesture.h>

#ifdef EMSCRIPTEN
#include <emscripten/html5.h>
#endif


namespace mirrage::input {

	using namespace util::unit_literals;

	class Input_manager::Gamepad {
	  public:
		Gamepad(Input_source src_id, SDL_GameController*, Input_mapper& mapper);
		~Gamepad();

		void force_feedback(float force);

		auto button_pressed(Pad_button b) const noexcept -> bool;
		auto button_released(Pad_button b) const noexcept -> bool;

		auto button_down(Pad_button b) const noexcept -> bool;
		auto button_up(Pad_button b) const noexcept -> bool;
		auto trigger(Pad_button b) const noexcept -> float;

		void update(util::Time dt);

		auto axis(Pad_stick stick) -> glm::vec2;

		auto id() const noexcept { return _id; }

	  private:
		Input_source        _src_id;
		int                 _id;
		SDL_GameController* _sdl_controller;
		SDL_Haptic*         _haptic;
		Input_mapper&       _mapper;

		uint8_t   _button_state[pad_button_count]{};
		float     _button_value[pad_button_count]{};
		glm::vec2 _stick_state[pad_stick_count]{};

		float _stick_dead_zone = 0.2f, _stick_max = 32767.f;

		float      _current_force = 0.f;
		util::Time _force_reset_timer{0.f};
	};

	namespace {
		inline auto from_sdl_keycode(SDL_Keycode key) { return static_cast<Key>(key); }

		inline auto to_sdl_pad_button(Pad_button b) { return static_cast<SDL_GameControllerButton>(b); }
		inline bool is_trigger(Pad_button b)
		{
			return b == Pad_button::left_trigger || b == Pad_button::right_trigger;
		}

		inline auto to_sdl_axis_x(Pad_stick s)
		{
			switch(s) {
				case Pad_stick::left: return SDL_CONTROLLER_AXIS_LEFTX;
				case Pad_stick::right: return SDL_CONTROLLER_AXIS_RIGHTX;
				default: return SDL_CONTROLLER_AXIS_INVALID;
			}
		}
		inline auto to_sdl_axis_y(Pad_stick s)
		{
			switch(s) {
				case Pad_stick::left: return SDL_CONTROLLER_AXIS_LEFTY;
				case Pad_stick::right: return SDL_CONTROLLER_AXIS_RIGHTY;
				default: return SDL_CONTROLLER_AXIS_INVALID;
			}
		}
	} // namespace


	Sdl_event_filter::Sdl_event_filter(Input_manager& e) : _manager(&e) { _manager->add_event_filter(*this); }
	Sdl_event_filter::Sdl_event_filter(Sdl_event_filter&& rhs) noexcept : _manager(rhs._manager)
	{
		if(_manager) {
			_manager->remove_event_filter(rhs);
			_manager->add_event_filter(*this);
		}
	}
	Sdl_event_filter& Sdl_event_filter::operator=(Sdl_event_filter&& rhs) noexcept
	{
		if(_manager)
			_manager->remove_event_filter(*this);

		if(rhs._manager)
			rhs._manager->remove_event_filter(rhs);

		_manager     = rhs._manager;
		rhs._manager = nullptr;

		if(_manager)
			_manager->add_event_filter(*this);

		return *this;
	}

	Sdl_event_filter::~Sdl_event_filter()
	{
		if(_manager) {
			_manager->remove_event_filter(*this);
		}
	}


	Input_manager::Input_manager(util::Message_bus& bus, asset::Asset_manager& assets)
	  : _mailbox(bus)
	  , _screen_to_world_coords(&util::identity<glm::vec2>)
	  , _mapper(std::make_unique<Input_mapper>(bus, assets))
	{

		SDL_JoystickEventState(SDL_ENABLE);
		SDL_GameControllerEventState(SDL_ENABLE);

		SDL_RecordGesture(-1);

		_mailbox.subscribe<Force_feedback, 4>(8, [&](auto& m) {
			auto handle = [&] {
				if(m.src > 0 && m.src - 1 < static_cast<int>(this->_gamepads.size())) {
					auto& pad = this->_gamepads.at(std::size_t(m.src - 1));
					if(pad)
						pad->force_feedback(m.force);
				}

#ifdef EMSCRIPTEN
				if(m.src == 0 && m.force > 0.5)
					emscripten_vibrate(200);
#endif
			};

			if(m.src >= 0)
				handle();

			else {
				for(auto i = 0u; i < this->_gamepads.size() + 1; i++) {
					m.src = gsl::narrow<Input_source>(i);
					handle();
				}
			}
		});
	}

	Input_manager::~Input_manager() noexcept {}

	void Input_manager::add_event_filter(Sdl_event_filter& f) { _event_filter.push_back(&f); }
	void Input_manager::remove_event_filter(Sdl_event_filter& f) { util::erase_fast(_event_filter, &f); }

	void Input_manager::capture_mouse(bool enable)
	{
		if(_mouse_captured != enable) {
			SDL_SetRelativeMouseMode(enable ? SDL_TRUE : SDL_FALSE);
			_mouse_captured = enable;

			if(!enable && _sdl_window) {
				SDL_WarpMouseInWindow(
				        _sdl_window, int(_mouse_capture_screen_pos.x), int(_mouse_capture_screen_pos.y));
			}
			_mouse_capture_screen_pos = _pointer_screen_pos[0];
		}
	}

	void Input_manager::update(util::Time dt)
	{
		_poll_events();

		for(auto i : util::range(std::size_t(_max_pointers)))
			_pointer_world_pos[i] = _screen_to_world_coords(_pointer_screen_pos[i]);

		for(auto& gp : _gamepads)
			if(gp) {
				gp->update(dt);
			}

		_mailbox.update_subscriptions();
	}

	void Input_manager::_poll_events()
	{
		for(auto& f : _event_filter) {
			f->pre_input_events();
		}

		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			auto filtered = std::find_if(_event_filter.begin(),
			                             _event_filter.end(),
			                             [&](auto f) { return !f->propagate(event); })
			                != _event_filter.end();

			if(!filtered) {
				_handle_event(event);
			}
		}

		for(auto& f : _event_filter) {
			f->post_input_events();
		}
	}


	void Input_manager::_handle_event(SDL_Event& event)
	{
		switch(event.type) {
			case SDL_TEXTINPUT: _mailbox.send<Char_input>(event.text.text); break;

			case SDL_KEYDOWN:
				if(event.key.repeat == 0)
					_mapper->on_key_pressed(from_sdl_keycode(event.key.keysym.sym));
				break;

			case SDL_KEYUP:
				if(event.key.repeat == 0)
					_mapper->on_key_released(from_sdl_keycode(event.key.keysym.sym));
				break;

			case SDL_MOUSEMOTION: _on_mouse_motion(event.motion); break;

			case SDL_MOUSEBUTTONDOWN:
				_mapper->on_mouse_button_pressed(event.button.button);
				_pointer_active[0] = true;
				break;

			case SDL_MOUSEBUTTONUP:
				_mapper->on_mouse_button_released(event.button.button,
				                                  static_cast<int8_t>(event.button.clicks));
				_pointer_active[0] = false;
				break;

			case SDL_FINGERMOTION:
			case SDL_FINGERUP:
			case SDL_FINGERDOWN: {
				auto idx = std::size_t(0);
				for(; idx < _max_pointers; idx++) {
					if(_pointer_finger_id[idx] == event.tfinger.fingerId)
						break;
				}
				if(idx >= _max_pointers) {
					for(idx = 0; idx < _max_pointers; idx++) {
						if(!_pointer_active[idx])
							break;
					}
				}

				if(idx < _max_pointers) {
					auto screen_pos = glm::vec2{event.tfinger.x, event.tfinger.y};
					if(screen_pos.x <= 1.0f && screen_pos.y <= 1.0f) {
						screen_pos *= glm::vec2{_viewport.z, _viewport.w};
						screen_pos += glm::vec2{_viewport.x, _viewport.y};
					}

					auto world_pos   = _screen_to_world_coords(screen_pos);
					auto screen_diff = screen_pos - _pointer_screen_pos[idx];
					auto world_diff  = world_pos - _pointer_world_pos[idx];

					_pointer_finger_id[idx]  = event.tfinger.fingerId;
					_pointer_screen_pos[idx] = screen_pos;
					_pointer_world_pos[idx]  = world_pos;
					_pointer_active[idx]     = event.type != SDL_FINGERUP;

					if(idx == 0) { //< mouse emulation
						if(_world_space_events) {
							_mapper->on_mouse_pos_change(world_diff, world_pos);
						} else {
							_mapper->on_mouse_pos_change(screen_diff, screen_pos);
						}

						if(event.tfinger.type == SDL_FINGERDOWN)
							_mapper->on_mouse_button_pressed(1, event.tfinger.pressure);
						else if(event.tfinger.type == SDL_FINGERUP)
							_mapper->on_mouse_button_released(1, 1);
					}
				}
				break;
			}

			case SDL_MOUSEWHEEL: _mapper->on_mouse_wheel_change({event.wheel.x, event.wheel.y}); break;

			case SDL_CONTROLLERDEVICEADDED: _add_gamepad(event.cdevice.which); break;

			case SDL_CONTROLLERDEVICEREMOVED: _remove_gamepad(event.cdevice.which); break;

			case SDL_CONTROLLERDEVICEREMAPPED: break; // ignored for now

			case SDL_DROPFILE: _mailbox.send<File_dropped>(event.drop.file); break;
		}
	}
	void Input_manager::_on_mouse_motion(const SDL_MouseMotionEvent& motion)
	{
		auto idx        = std::size_t(0);
		auto screen_pos = _mouse_captured ? _pointer_screen_pos.at(idx) + glm::vec2{motion.xrel, motion.yrel}
		                                  : glm::vec2{motion.x, motion.y};
		auto world_pos   = _screen_to_world_coords(screen_pos);
		auto screen_diff = screen_pos - _pointer_screen_pos.at(idx);
		auto world_diff  = world_pos - _pointer_world_pos[idx];

		screen_diff.x = screen_pos.x - _pointer_screen_pos.at(idx).x;
		screen_diff.y = screen_pos.y - _pointer_screen_pos.at(idx).y;

		world_diff.x = world_pos.x - _pointer_world_pos.at(idx).x;
		world_diff.y = world_pos.y - _pointer_world_pos.at(idx).y;

		if(_mouse_captured) {
			screen_pos = _pointer_screen_pos[idx];
			world_pos  = _pointer_world_pos[idx];
		} else {
			screen_pos = glm::vec2{motion.x, motion.y};
			world_pos  = _screen_to_world_coords(_pointer_screen_pos[idx]);
		}

		_pointer_screen_pos[idx] = screen_pos;
		_pointer_world_pos[idx]  = world_pos;

		if(_world_space_events) {
			_mapper->on_mouse_pos_change(world_diff, world_pos);
		} else {
			_mapper->on_mouse_pos_change(screen_diff, screen_pos);
		}
	}

	void Input_manager::_add_gamepad(int joystick_id)
	{
		if(joystick_id == -1)
			joystick_id = SDL_NumJoysticks() - 1;

		if(SDL_IsGameController(joystick_id)) {
			SDL_GameController* controller = SDL_GameControllerOpen(joystick_id);
			if(controller) {
				_gamepads.emplace_back(std::make_unique<Gamepad>(_gamepads.size() + 1, controller, *_mapper));
				_mailbox.send<Source_added>(Input_source(_gamepads.size()));
			} else {
				std::cerr << "Could not open gamecontroller " << joystick_id << ": " << SDL_GetError()
				          << std::endl;
			}
		}
	}

	void Input_manager::_remove_gamepad(int instance_id)
	{
		auto e = std::find_if(_gamepads.begin(), _gamepads.end(), [instance_id](auto& c) {
			return c->id() == instance_id;
		});

		if(e != _gamepads.end()) {
			auto idx = std::distance(_gamepads.begin(), e);
			_mailbox.send<Source_removed>(Input_source(idx));
			e->reset();
		}
	}

	void Input_manager::enable_context(Context_id id) { _mapper->enable_context(id); }


	// Gamepad impl
	Input_manager::Gamepad::Gamepad(Input_source src_id, SDL_GameController* c, Input_mapper& mapper)
	  : _src_id(src_id)
	  , _id(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c)))
	  , _sdl_controller(c)
	  , _mapper(mapper)
	{

		auto name = SDL_GameControllerName(_sdl_controller);

		_haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(_sdl_controller));
		if(!_haptic)
			LOG(plog::warning) << "Warning: Controller '" << name
			                   << "'' does not support haptics: " << SDL_GetError();

		else {
			if(SDL_HapticRumbleInit(_haptic) < 0) {
				_haptic = nullptr;
				LOG(plog::warning) << "Warning: Unable to initialize rumble for '" << name
				                   << "': " << SDL_GetError();
			}
		}

		LOG(plog::info) << "Detected gamepad '" << name << "'";
	}
	Input_manager::Gamepad::~Gamepad()
	{
		if(_haptic)
			SDL_HapticClose(_haptic);

		SDL_GameControllerClose(_sdl_controller);
	}

	void Input_manager::Gamepad::force_feedback(float force)
	{
		if(force > _current_force) {
			_force_reset_timer = 200_ms;
			_current_force     = force;
		}

		if(_haptic)
			SDL_HapticRumblePlay(_haptic, glm::clamp(_current_force, 0.f, 1.f), 200);
	}

	auto Input_manager::Gamepad::button_pressed(Pad_button b) const noexcept -> bool
	{
		return _button_state[static_cast<int8_t>(b)] == 1;
	}
	auto Input_manager::Gamepad::button_released(Pad_button b) const noexcept -> bool
	{
		return _button_state[static_cast<int8_t>(b)] == 3;
	}

	auto Input_manager::Gamepad::trigger(Pad_button b) const noexcept -> float
	{
		auto state = [&](auto t) {
			return glm::abs(SDL_GameControllerGetAxis(_sdl_controller, t)) / _stick_max;
		};

		if(b == Pad_button::left_trigger)
			return state(SDL_CONTROLLER_AXIS_TRIGGERLEFT);

		else if(b == Pad_button::right_trigger)
			return state(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

		else
			return 0;
	}

	auto Input_manager::Gamepad::button_down(Pad_button button) const noexcept -> bool
	{
		if(!is_trigger(button))
			return SDL_GameControllerGetButton(_sdl_controller, to_sdl_pad_button(button)) != 0;

		else
			return trigger(button) > _stick_dead_zone;
	}
	auto Input_manager::Gamepad::button_up(Pad_button b) const noexcept -> bool { return !button_down(b); }

	void Input_manager::Gamepad::update(util::Time dt)
	{
		if(_force_reset_timer > 0_s) {
			_force_reset_timer -= dt;
			if(_force_reset_timer < 0_s)
				_current_force = 0.f;
		}

		for(auto i : util::range(pad_button_count)) {
			auto button = static_cast<Pad_button>(i);
			bool down   = button_down(button);

			switch(_button_state[i]) {
				case 0: // up
					_button_state[i] = down ? 1 : 0;
					break;
				case 1: // pressed
					_button_state[i] = down ? 2 : 3;
					if(is_trigger(button))
						_mapper.on_pad_button_pressed(_src_id, button, trigger(button));
					else
						_mapper.on_pad_button_pressed(_src_id, button, 1.f);

					break;
				case 2: // down
					_button_state[i] = down ? 2 : 3;
					{
						auto value = trigger(button);
						if(std::abs(_button_value[i] - value) > 0.1f) {
							_mapper.on_pad_button_changed(_src_id, button, value);
							_button_value[i] = value;
						}
					}

					break;
				case 3: // released
					_button_state[i] = down ? 1 : 0;
					_mapper.on_pad_button_released(_src_id, button);
					break;
			}
		}

		for(auto i : util::range(pad_stick_count)) {
			auto stick = static_cast<Pad_stick>(i);

			auto state = axis(stick);
			if(state != _stick_state[i]) {
				_mapper.on_pad_stick_change(_src_id, stick, state - _stick_state[i], state);
				_stick_state[i] = state;
			}
		}
	}

	auto Input_manager::Gamepad::axis(Pad_stick s) -> glm::vec2
	{
		glm::vec2 v{
		        SDL_GameControllerGetAxis(_sdl_controller, to_sdl_axis_x(s)) / _stick_max,
		        SDL_GameControllerGetAxis(_sdl_controller, to_sdl_axis_y(s)) / _stick_max,
		};

		auto dz     = _stick_dead_zone;
		auto length = glm::length(v);

		if(length < dz)
			return glm::vec2{0, 0};

		return v / length * ((length - dz) / (1 - dz));
	}
} // namespace mirrage::input
