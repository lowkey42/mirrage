/** stacktrace generator & error handler *************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <stdexcept>
#include <string>

namespace mirrage::util {

	extern void init_stacktrace(std::string exe_path);

	extern bool is_stacktrace_available();

	extern std::string gen_stacktrace(int frames_to_skip = 0);

	struct Error : public std::runtime_error {
		explicit Error(const std::string& msg) : std::runtime_error(msg + "\n At " + gen_stacktrace(1)) {}
	};
}
