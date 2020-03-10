#include <mirrage/engine.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/audio/audio_manager.hpp>
#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/window.hpp>
#include <mirrage/gui/gui.hpp>
#include <mirrage/input/input_manager.hpp>
#include <mirrage/net/net_manager.hpp>
#include <mirrage/translations.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/time.hpp>
#include <mirrage/utils/units.hpp>
#include <mirrage/utils/console_command.hpp>

#include <chrono>
#include <stdexcept>

#ifdef _WIN32
// TODO
#else
#include <signal.h>
#endif

extern void ref_embedded_assets_mirrage();

namespace mirrage {
	namespace {
		void init_sub_system(Uint32 f, const std::string& name, bool required = true)
		{
			if(SDL_InitSubSystem(f) != 0) {
				auto m = "Could not initialize " + name + ": " + get_sdl_error();

				if(required)
					MIRRAGE_FAIL(m);
				else
					LOG(plog::warning) << m;
			}
		}

		auto interruptRequested = false;
		void sigIntHandler(int s) { interruptRequested = true; }

	} // namespace

	using namespace util::unit_literals;

	struct Engine_event_filter : public input::Sdl_event_filter {
		Engine& _engine;

		Engine_event_filter(Engine& engine) : Sdl_event_filter(engine.input()), _engine(engine) {}

		bool propagate(SDL_Event& event) override
		{
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

	auto get_sdl_error() -> std::string
	{
		std::string sdl_error(SDL_GetError());
		SDL_ClearError();
		return sdl_error;
	}

	Engine::Sdl_wrapper::Sdl_wrapper(bool headless)
	{
		MIRRAGE_INVARIANT(SDL_Init(0) == 0, "Could not initialize SDL: " << get_sdl_error());

		if(!headless) {
			init_sub_system(SDL_INIT_AUDIO, "SDL_Audio");
			init_sub_system(SDL_INIT_VIDEO, "SDL_Video");
			init_sub_system(SDL_INIT_JOYSTICK, "SDL_Joystick");
			init_sub_system(SDL_INIT_HAPTIC, "SDL_Haptic", false);
			init_sub_system(SDL_INIT_GAMECONTROLLER, "SDL_Gamecontroller");
		}
		init_sub_system(SDL_INIT_EVENTS, "SDL_Events");
	}
	Engine::Sdl_wrapper::~Sdl_wrapper() { SDL_Quit(); }


	Engine::Engine(const std::string&       org,
	               const std::string&       title,
	               util::maybe<std::string> base_dir,
	               std::uint32_t            version_major,
	               std::uint32_t            version_minor,
	               bool                     debug,
	               bool                     headless,
	               int                      argc,
	               char**                   argv,
	               char**)
	  : _screens(*this)
	  , _asset_manager(std::make_unique<asset::Asset_manager>(argc > 0 ? argv[0] : "", org, title, base_dir))
	  , _translator(std::make_unique<Translator>(*_asset_manager))
	  , _sdl(headless)
	  , _graphics_context(headless ? std::unique_ptr<graphic::Context>()
	                               : std::make_unique<graphic::Context>(
	                                       title,
	                                       graphic::make_version_number(version_major, version_minor, 0),
	                                       "No Engine",
	                                       graphic::make_version_number(0, 0, 0),
	                                       debug,
	                                       *_asset_manager))
	  , _graphics_main_window(headless ? util::nothing : _graphics_context->find_window("Main"))
	  , _input_manager(headless ? std::unique_ptr<input::Input_manager>()
	                            : std::make_unique<input::Input_manager>(_bus, *_asset_manager))
	  , _event_filter(headless ? std::unique_ptr<Engine_event_filter>()
	                           : std::make_unique<Engine_event_filter>(*this))
	  , _net_manager(std::make_unique<net::Net_manager>())
	  , _audio_manager(std::make_unique<audio::Audio_manager>(*_asset_manager, !headless))
	  , _console_commands(std::make_unique<util::Console_command_container>())
	  , _current_time(SDL_GetTicks() / 1000.0)
	  , _headless(headless)
	{
		ref_embedded_assets_mirrage();

		_graphics_main_window.process([&](auto& window) {
			_input_manager->viewport({0, 0, window.width(), window.height()});
			_input_manager->window(window.window_handle());

			_gui = std::make_unique<gui::Gui>(window.viewport(), *_asset_manager, *_input_manager);
		});
		
		_console_commands->add("screen.leave <count> | Pops the top <count> screens",
		                     [&](std::uint8_t depth) { screens().leave(depth); });
		                     
		_console_commands->add(
		        "screen.print | Prints the currently open screens (=> update+draw next, D> only draw, S> "
		        "don't update+draw)",
		        [&]() {
			        auto screen_list = screens().print_stack();
			        LOG(plog::info) << "Open Screens: " << screen_list;
		        });

		if(headless) {
#ifdef _WIN32
			// TODO
			(void) (&sigIntHandler);
#else
			struct sigaction action;
			action.sa_handler = sigIntHandler;
			sigemptyset(&action.sa_mask);
			action.sa_flags = 0;

			sigaction(SIGINT, &action, nullptr);
			sigaction(SIGTERM, &action, nullptr);
#endif
		}
	}

	Engine::~Engine() noexcept
	{
		_screens.clear();

		assets().shrink_to_fit();
	}

	namespace {
		float smooth(float curr)
		{
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
	} // namespace

	void Engine::on_frame()
	{
		_last_time               = _current_time;
		_current_time            = util::current_time_sec();
		auto delta_time          = std::max(0.f, static_cast<float>(_current_time - _last_time));
		auto delta_time_smoothed = smooth(std::min(delta_time, 1.f / 1));

		if(_graphics_main_window.is_some() && _input_manager) {
			_input_manager->viewport(_graphics_main_window.get_or_throw().viewport());
		}

		if(interruptRequested) {
			interruptRequested = false;
			exit();
		}

		if(_input_manager) {
			_input_manager->update(delta_time * second);
		}

		_on_pre_frame(delta_time_smoothed * second);

		if(_gui)
			_gui->start_frame();

		_screens.on_frame(delta_time_smoothed * second);

		_audio_manager->update(delta_time * second);
		_on_post_frame(delta_time_smoothed * second);

		_screens.do_queued_actions();
	}
} // namespace mirrage
