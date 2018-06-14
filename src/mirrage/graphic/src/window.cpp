#include <mirrage/graphic/window.hpp>

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/device.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/time.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <gsl/gsl>
#include <sf2/sf2.hpp>

#include <cstdio>
#include <iostream>
#include <sstream>


namespace mirrage::graphic {

	using namespace util::unit_literals;

	namespace {
		void sdl_error_check()
		{
			const char* err = SDL_GetError();
			if(*err != '\0') {
				std::string errorStr(err);
				SDL_ClearError();
				MIRRAGE_FAIL("SDL: " << errorStr);
			}
		}

		auto sdl_fullscreen_flag(Fullscreen f) -> SDL_WindowFlags
		{
			if(f == Fullscreen::no) {
				return SDL_WindowFlags(0);
			}

			return f == Fullscreen::yes ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
	} // namespace

	Window::Window(
	        std::string name, std::string title, int display, int width, int height, Fullscreen fullscreen)
	  : _name(name)
	  , _title(title)
	  , _display(display)
	  , _width(width)
	  , _height(height)
	  , _fullscreen(fullscreen)
	  , _window(nullptr, SDL_DestroyWindow)
	{

		auto win_flags = Uint32(SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
		                        | sdl_fullscreen_flag(fullscreen));

		auto display_uint = gsl::narrow<unsigned int>(display);

		_window.reset(SDL_CreateWindow(_title.c_str(),
		                               int(SDL_WINDOWPOS_CENTERED_DISPLAY(display_uint)),
		                               int(SDL_WINDOWPOS_CENTERED_DISPLAY(display_uint)),
		                               width,
		                               height,
		                               win_flags));

		if(!_window)
			MIRRAGE_FAIL("Unable to create window");

		sdl_error_check();
	}
	Window::~Window() = default;

	void Window::create_surface(Context& context)
	{
		if(_surface)
			return;

		auto surface = VkSurfaceKHR{};
		SDL_Vulkan_CreateSurface(_window.get(), context.instance(), &surface);
		_surface = vk::UniqueSurfaceKHR(surface, {context.instance()});
		sdl_error_check();
	}


	void Window::on_present()
	{
		auto present_started = util::current_time_sec();

		_update_fps_timer(present_started);
	}

	void Window::_update_fps_timer(double)
	{
		auto delta_time = static_cast<float>(util::current_time_sec() - _frame_start_time);

		float smooth_factor  = 0.1f;
		_delta_time_smoothed = (1.0f - smooth_factor) * _delta_time_smoothed + smooth_factor * delta_time;

		_time_since_last_FPS_output += delta_time;
		if(_time_since_last_FPS_output >= 1.0f) {
			_time_since_last_FPS_output = 0.0f;
			std::ostringstream osstr;
			osstr << _title << " (" << (int((1.0f / _delta_time_smoothed) * 10.0f) / 10.0f) << " FPS, ";
			osstr << (int(_delta_time_smoothed * 10000.0f) / 10.0f) << " ms/frame)";

			SDL_SetWindowTitle(_window.get(), osstr.str().c_str());
		}

		_frame_start_time = util::current_time_sec();
	}

	void Window::title(const std::string& title)
	{
		_title = title;
		SDL_SetWindowTitle(_window.get(), title.c_str());
	}

	bool Window::dimensions(int& width, int& height, Fullscreen fullscreen)
	{
		SDL_DisplayMode target, closest;
		target.w            = width;
		target.h            = height;
		target.format       = 0;
		target.refresh_rate = 0;
		target.driverdata   = nullptr;

		if(fullscreen == Fullscreen::yes) { // check capabilities
			if(!SDL_GetClosestDisplayMode(_display, &target, &closest)) {
				return false;
			}

			width  = closest.w;
			height = closest.h;
		}

		SDL_SetWindowSize(_window.get(), width, height);

		if(_fullscreen != fullscreen) {
			_fullscreen = fullscreen;

			const auto fs_type = [&]() -> Uint32 {
				switch(fullscreen) {
					case Fullscreen::no: return 0;
					case Fullscreen::yes: return SDL_WINDOW_FULLSCREEN;
					case Fullscreen::yes_borderless: return SDL_WINDOW_FULLSCREEN_DESKTOP;
				}
				MIRRAGE_FAIL("Unexpected fullscreen enum value: " << static_cast<int>(fullscreen));
			}();
			SDL_SetWindowFullscreen(_window.get(), fs_type);

			if(fullscreen != Fullscreen::no) {
				SDL_SetWindowDisplayMode(_window.get(), &closest);
			} else {
				SDL_SetWindowSize(_window.get(), width, height);
			}
		}

		SDL_GetWindowSize(_window.get(), &width, &height);
		_width  = width;
		_height = height;

		sdl_error_check();

		// report changed size to renderer => change viewport
		for(auto& listener : util::Registration<Window, Window_modification_handler>::children()) {
			listener->on_window_modified(*this);
		}

		return true;
	}
} // namespace mirrage::graphic
