#pragma once

#include <mirrage/utils/units.hpp>

#include <unordered_map>
#include <memory>
#include <string>


namespace mirrage {
namespace graphic {

	class Context;
	class Device;
	class Window;

	using Device_ptr = std::unique_ptr<Device>;
	using Window_ptr = std::unique_ptr<Window>;


	enum class Fullscreen {
		no,
		yes,
		yes_borderless
	};

	struct Window_settings {
		int width;
		int height;
		int display;
		Fullscreen fullscreen;
	};

	struct Graphics_settings {
		std::unordered_map<std::string, Window_settings> windows;
	};


	inline auto default_window_settings(int display=0) -> Window_settings {
		return Window_settings{1280, 720, display, Fullscreen::no};
	}
	inline auto default_settings(int display=0) -> Graphics_settings {
		auto settings = Graphics_settings{};
		settings.windows["Main"] = default_window_settings(display);

		return settings;
	}

	inline constexpr auto make_version_number(std::uint32_t major, std::uint32_t minor, std::uint32_t patch) {
		return (((major) << 22) | ((minor) << 12) | (patch));
	}


#ifdef sf2_structDef
	sf2_enumDef(Fullscreen, no, yes, yes_borderless)
	sf2_structDef(Window_settings,
		width,
		height,
		display,
		fullscreen
	)
	sf2_structDef(Graphics_settings,
		windows
	)
#endif

}
}

