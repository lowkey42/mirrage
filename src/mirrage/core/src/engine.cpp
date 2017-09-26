#include <mirrage/engine.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/window.hpp>
#include <mirrage/input/input_manager.hpp>
#include <mirrage/translations.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/time.hpp>
#include <mirrage/utils/units.hpp>

#include <chrono>
#include <stdexcept>


namespace mirrage {
	namespace {
		void init_sub_system(Uint32 f, const std::string& name, bool required = true) {
			if(SDL_InitSubSystem(f) != 0) {
				auto m = "Could not initialize " + name + ": " + get_sdl_error();

				if(required)
					FAIL(m);
				else
					WARN(m);
			}
		}
	}

	using namespace util::unit_literals;

	struct Engine_event_filter : public input::Sdl_event_filter {
		Engine& _engine;

		Engine_event_filter(Engine& engine) : Sdl_event_filter(engine.input()), _engine(engine) {}

		bool propagate(SDL_Event& event) override {
			if(event.type == SDL_QUIT) {
				_engine.exit();
				return false;

			} else if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F12) {
				_engine.assets().reload();
				return false;
			}

			return true;
		}
	};

	auto get_sdl_error() -> std::string {
		std::string sdl_error(SDL_GetError());
		SDL_ClearError();
		return sdl_error;
	}

	Engine::Sdl_wrapper::Sdl_wrapper() {
		INVARIANT(SDL_Init(0) == 0, "Could not initialize SDL: " << get_sdl_error());

		init_sub_system(SDL_INIT_VIDEO, "SDL_Video");
		init_sub_system(SDL_INIT_JOYSTICK, "SDL_Joystick");
		init_sub_system(SDL_INIT_HAPTIC, "SDL_Haptic", false);
		init_sub_system(SDL_INIT_GAMECONTROLLER, "SDL_Gamecontroller");
		init_sub_system(SDL_INIT_EVENTS, "SDL_Events");
	}
	Engine::Sdl_wrapper::~Sdl_wrapper() { SDL_Quit(); }


	Engine::Engine(const std::string& title,
	               std::uint32_t      version_major,
	               std::uint32_t      version_minor,
	               bool               debug,
	               int                argc,
	               char**             argv,
	               char**)
	  : _screens(*this)
	  , _asset_manager(std::make_unique<asset::Asset_manager>(argc > 0 ? argv[0] : "", title))
	  , _translator(std::make_unique<Translator>(*_asset_manager))
	  , _sdl()
	  , _graphics_context(std::make_unique<graphic::Context>(
	            title,
	            graphic::make_version_number(version_major, version_minor, 0),
	            "No Engine",
	            graphic::make_version_number(0, 0, 0),
	            debug,
	            *_asset_manager))
	  , _graphics_main_window(_graphics_context->create_window("Main"))
	  , _input_manager(std::make_unique<input::Input_manager>(_bus, *_asset_manager))
	  , _event_filter(std::make_unique<Engine_event_filter>(*this))
	  , _current_time(SDL_GetTicks() / 1000.0) {

		if(_graphics_main_window) {
			_input_manager->viewport({0, 0, _graphics_main_window->width(), _graphics_main_window->height()});
			_input_manager->window(_graphics_main_window->window_handle());
		}
	}

	Engine::~Engine() noexcept {
		_screens.clear();

		assets().shrink_to_fit();
	}

	namespace {
		float smooth(float curr) {
			static float history[11]{};
			static auto  i     = 0;
			static auto  first = true;
			if(first) {
				first = false;
				std::fill(std::begin(history), std::end(history), 1.f / 60);
			}

			auto sum = 0.f;
			auto min = 999.f, max = 0.f;
			for(auto v : history) {
				if(v < min) {
					if(min < 999.f) {
						sum += min;
					}
					min = v;
				} else if(v > max) {
					sum += max;
					max = v;
				} else {
					sum += v;
				}
			}
			curr = glm::mix(sum / 9.f, curr, 0.2f);

			i          = (i + 1) % 11;
			history[i] = curr;
			return curr;
		}
	}

	void Engine::on_frame() {
		_last_time               = _current_time;
		_current_time            = util::current_time_sec();
		auto delta_time          = std::max(0.f, static_cast<float>(_current_time - _last_time));
		auto delta_time_smoothed = smooth(std::min(delta_time, 1.f / 1));

		if(_graphics_main_window) {
			_input_manager->viewport(_graphics_main_window->viewport());
		}

		_bus.update();

		_input_manager->update(delta_time * second);

		_on_pre_frame(delta_time_smoothed * second);

		_screens.on_frame(delta_time_smoothed * second);

		_on_post_frame(delta_time_smoothed * second);

		_screens.do_queued_actions();
	}
}
