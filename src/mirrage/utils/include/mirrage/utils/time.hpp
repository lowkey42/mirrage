/** Helper function for timekeeping and in-app profiling *********************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#ifdef SDL_FRAMETIME
#include <SDL2/SDL.h>
#else
#include <chrono>
#endif

namespace mirrage::util {

	inline double current_time_sec() {
#ifdef SDL_FRAMETIME
		return static_cast<float>(SDL_GetTicks()) / 1000.f;
#else
		using namespace std::chrono;
		static const auto start_time = high_resolution_clock::now();
		return duration_cast<duration<double, std::micro>>(high_resolution_clock::now() - start_time).count()
		       / 1000.0 / 1000.0;
#endif
	}
}
