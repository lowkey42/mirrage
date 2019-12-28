#pragma once

#include "common.hpp"
#include "helper/progress.hpp"

#include <assimp/scene.h>

#include <string>

namespace mirrage {

	extern bool convert_material(const std::string&           name,
	                             const aiMaterial&            material,
	                             const std::string&           base_dir,
	                             const std::string&           output,
	                             const Mesh_converted_config& cfg,
	                             helper::Progress_container&  progress,
	                             bool                         interactive,
	                             bool                         interactive_all);

	extern void convert_texture(const std::string&              input,
	                            const util::maybe<std::string>& name,
	                            const std::string&              output_dir,
	                            bool                            normal_texture,
	                            bool                            srgb,
	                            int                             dxt_level,
	                            helper::Progress_container&     progress);

} // namespace mirrage
