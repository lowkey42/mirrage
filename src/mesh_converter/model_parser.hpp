#pragma once

#include "common.hpp"

#include <string>

namespace mirrage {

	extern void convert_model(const std::string&           path,
	                          const std::string&           output,
	                          const Mesh_converted_config& cfg);
}
