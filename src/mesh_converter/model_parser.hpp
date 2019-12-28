#pragma once

#include "common.hpp"
#include "helper/progress.hpp"

#include <string>

namespace mirrage {

	extern void convert_model(const std::string&              path,
	                          const util::maybe<std::string>& name,
	                          const std::string&              output,
	                          const Mesh_converted_config&    cfg,
	                          util::maybe<std::string>        fit_skeleton,
	                          bool                            guess_scale,
	                          const std::vector<std::string>& material_names,
	                          helper::Progress_container&     progress,
	                          bool                            ansi);
}
