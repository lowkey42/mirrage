#include <mirrage/graphic/settings.hpp>

#include <SDL.h>


namespace mirrage::graphic {

	auto default_window_settings(int display) -> Window_settings
	{
#ifdef NDEBUG
		auto mode = SDL_DisplayMode{SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0};
		if(SDL_GetDisplayMode(display, 0, &mode) == 0) {
			return Window_settings{mode.w, mode.h, display, Fullscreen::yes};
		}
#endif

		return Window_settings{1280, 720, display, Fullscreen::no};
	}

} // namespace mirrage::graphic
