#pragma once

#include <assimp/scene.h>

#include <string>

namespace mirrage {

	extern bool convert_material(const std::string& name,
	                             const aiMaterial&  material,
	                             const std::string& base_dir,
	                             const std::string& output);
}
