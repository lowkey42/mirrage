#pragma once

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

struct aiScene;
struct aiNode;
struct aiBone;

namespace mirrage {
	struct Mesh_converted_config;

	struct Bone_data {
		const aiNode* assimp_node;
		const aiBone* assimp_bone;

		std::string name;
		int         idx = -1;
		std::string parent_name;
		int         parent_idx           = -1;
		glm::mat4   offset               = glm::mat4(1);
		glm::mat4   node_transform       = glm::mat4(1);
		glm::mat4   local_node_transform = glm::mat4(1);

		Bone_data() = default;
		Bone_data(const aiNode& node,
		          glm::mat4     node_transform,
		          glm::mat4     loca_node_transform,
		          int           idx,
		          int           parent_idx);
	};
	struct Skeleton_data {
		std::vector<Bone_data>               bones;
		std::unordered_map<std::string, int> bones_by_name;
	};

	extern auto parse_skeleton(const std::string&           model_name,
	                           const std::string&           output,
	                           const aiScene&               scene,
	                           const Mesh_converted_config& cfg) -> Skeleton_data;
} // namespace mirrage
