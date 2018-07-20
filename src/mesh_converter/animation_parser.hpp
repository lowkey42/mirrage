#pragma once

#include <string>

struct aiScene;

namespace mirrage {
	struct Skeleton_data;
	struct Mesh_converted_config;

	extern void parse_animations(const std::string&           model_name,
	                             const std::string&           output,
	                             const aiScene&               scene,
	                             const Mesh_converted_config& cfg,
	                             const Skeleton_data&);

} // namespace mirrage
