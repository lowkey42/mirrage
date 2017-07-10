#include <mirrage/graphic/window.hpp>

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/device.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/time.hpp>

#include <sf2/sf2.hpp>
#include <SDL2/SDL.h>
#include <SDL_vulkan.h>
#include <gsl/gsl>

#include <iostream>
#include <sstream>
#include <cstdio>


namespace lux {
namespace graphic {

	using namespace util::unit_literals;

	namespace {
		void sdl_error_check() {
			const char *err = SDL_GetError();
			if(*err != '\0') {
				std::string errorStr(err);
				SDL_ClearError();
				FAIL("SDL: "<<errorStr);
			}
		}

		auto sdl_fullscreen_flag(Fullscreen f) -> SDL_WindowFlags {
			if(f==Fullscreen::no) {
				return SDL_WindowFlags(0);
			}

			return f==Fullscreen::yes ? SDL_WINDOW_FULLSCREEN
			                          : SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
	}

	Window::Window(Context& context, std::string name, std::string title, int display, int width, int height,
	               Fullscreen fullscreen)
	 : util::Registered<Window,Context>(context), _name(name), _title(title), _display(display)
	 ,_width(width), _height(height), _fullscreen(fullscreen), _window(nullptr, SDL_DestroyWindow) {

		auto win_flags = Uint32(SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI |
		                        sdl_fullscreen_flag(fullscreen));

		auto display_uint = gsl::narrow<unsigned int>(display);

		_window.reset(SDL_CreateWindow(_title.c_str(),
		                               int(SDL_WINDOWPOS_CENTERED_DISPLAY(display_uint)),
		                               int(SDL_WINDOWPOS_CENTERED_DISPLAY(display_uint)),
		                               width, height, win_flags) );

		if (!_window)
			FAIL("Unable to create window");

		sdl_error_check();

		auto surface = VkSurfaceKHR{};
		SDL_CreateVulkanSurface(_window.get(), context.instance(), &surface);
		_surface = vk::UniqueSurfaceKHR(surface, vk::SurfaceKHRDeleter{context.instance()});

	}
	Window::~Window() = default;

	void Window::on_present() {
		auto present_started = util::current_time_sec();

		_update_fps_timer(present_started);
	}

	void Window::_update_fps_timer(double present_started) {
		auto delta_time = static_cast<float>(util::current_time_sec() - _frame_start_time);

		float smooth_factor=0.1f;
		_delta_time_smoothed=(1.0f-smooth_factor)*_delta_time_smoothed+smooth_factor*delta_time;

		auto cpu_delta_time = static_cast<float>(present_started - _frame_start_time);
		_cpu_delta_time_smoothed=(1.0f-smooth_factor)*_cpu_delta_time_smoothed+smooth_factor*cpu_delta_time;

		_time_since_last_FPS_output+=delta_time;
		if(_time_since_last_FPS_output>=1.0f){
			_time_since_last_FPS_output=0.0f;
			std::ostringstream osstr;
			osstr<<_title<<" ("<<(int((1.0f/_delta_time_smoothed)*10.0f)/10.0f)<<" FPS, ";
			osstr<<(int(_delta_time_smoothed*10000.0f)/10.0f)<<" ms/frame, ";
			osstr<<(int(_cpu_delta_time_smoothed*10000.0f)/10.0f)<<" ms/frame [cpu])";

			SDL_SetWindowTitle(_window.get(), osstr.str().c_str());
		}

		_frame_start_time = util::current_time_sec();
	}

	void Window::title(const std::string& title) {
		_title = title;
		SDL_SetWindowTitle(_window.get(), title.c_str());
	}

	bool Window::dimensions(int& width, int& height, Fullscreen fullscreen) {
		SDL_DisplayMode target, closest;
		target.w = width;
		target.h = height;
		target.format       = 0;
		target.refresh_rate = 0;
		target.driverdata   = 0;

		if(fullscreen==Fullscreen::yes) { // check capabilities
			if(!SDL_GetClosestDisplayMode(_display, &target, &closest)) {
				return false;
			}

			width = closest.w;
			height = closest.h;
		}

		SDL_SetWindowSize(_window.get(), width, height);

		if(_fullscreen!=fullscreen) {
			_fullscreen = fullscreen;

			auto fs_type = fullscreen==Fullscreen::yes ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
			SDL_SetWindowFullscreen(_window.get(), fs_type);
			SDL_SetWindowDisplayMode(_window.get(), &closest);
		}

		SDL_GetWindowSize(_window.get(), &width, &height);
		_width  = width;
		_height = height;

		sdl_error_check();

		// TODO: report changed size to renderer => change viewport

		return true;
	}

}
}
