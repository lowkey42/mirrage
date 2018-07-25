#include "skeleton_parser.hpp"

#include "common.hpp"

#include <mirrage/renderer/model.hpp>
#include <mirrage/utils/log.hpp>

#include <assimp/scene.h>
#include <glm/gtx/string_cast.hpp>
#include <gsl/gsl>

#include <fstream>
#include <iostream>
#include <vector>


namespace mirrage {

	Bone_data::Bone_data(const aiNode&            node,
	                     renderer::Bone_transform local_node_transform,
	                     int                      idx,
	                     int                      parent_idx)
	  : assimp_node(&node)
	  , assimp_bone(nullptr)
	  , name(node.mName.C_Str())
	  , idx(idx)
	  , parent_name(node.mParent ? node.mParent->mName.C_Str() : "")
	  , parent_idx(parent_idx)
	  , local_node_transform(local_node_transform)
	{
	}


	auto parse_skeleton(const std::string&           model_name,
	                    const std::string&           output,
	                    const aiScene&               scene,
	                    const Mesh_converted_config& cfg) -> Skeleton_data
	{
		auto skeleton = Skeleton_data{};
		// first pass over the entire scene graph and add each node as a possible bone
		auto map_node = [&](const aiNode* node, auto parent_idx, auto&& recurse) -> void {
			auto idx = skeleton.bones.size();
			skeleton.bones_by_name.emplace(node->mName.C_Str(), idx);

			skeleton.bones.emplace_back(
			        *node, renderer::to_bone_transform(to_glm(node->mTransformation)), idx, parent_idx);

			for(auto& c : gsl::span(node->mChildren, node->mNumChildren)) {
				recurse(c, idx, recurse);
			}
		};
		map_node(scene.mRootNode, -1, map_node);

		// pass over all meshes and add reference to aiBone
		auto used_bones = std::vector<bool>(skeleton.bones.size(), false);
		auto map_meshes = [&](const aiNode* node, auto parent_transform, auto&& recurse) -> void {
			auto transform     = parent_transform * to_glm(node->mTransformation);
			auto inv_transform = glm::inverse(transform);

			for(auto& c : gsl::span(node->mChildren, node->mNumChildren)) {
				recurse(c, transform, recurse);
			}

			for(auto& mesh_idx : gsl::span(node->mMeshes, node->mNumMeshes)) {
				auto mesh = scene.mMeshes[mesh_idx];
				if(mesh->HasBones()) {
					for(auto& bone : gsl::span(mesh->mBones, mesh->mNumBones)) {
						auto node = skeleton.bones_by_name.find(bone->mName.C_Str());
						if(node != skeleton.bones_by_name.end()) {
							auto& bone_data       = skeleton.bones[std::size_t(node->second)];
							bone_data.assimp_bone = bone;
							bone_data.offset =
							        renderer::to_bone_transform(to_glm(bone->mOffsetMatrix) * inv_transform);

							// mark bone and all parents as used
							used_bones[std::size_t(bone_data.idx)] = true;
							for(auto parent_idx = bone_data.parent_idx; parent_idx != -1;) {
								used_bones[std::size_t(parent_idx)] = true;
								parent_idx = skeleton.bones[std::size_t(parent_idx)].parent_idx;
							}

						} else {
							LOG(plog::warning) << "Bone " << bone->mName.C_Str() << " in model " << model_name
							                   << " references an unknown node.";
						}
					}
				}
			}
		};
		map_meshes(scene.mRootNode, glm::mat4(1), map_meshes);

		// also mark some selected bones used for IK or as anchors
		for(auto& bone_name : cfg.empty_bones_to_keep) {
			util::find_maybe(skeleton.bones_by_name, bone_name).process([&](auto& bone_idx) {
				// mark bone and all parents as used
				auto& bone_data                   = skeleton.bones[std::size_t(bone_idx)];
				used_bones[std::size_t(bone_idx)] = true;

				for(auto parent_idx = bone_data.parent_idx; parent_idx != -1;) {
					used_bones[std::size_t(parent_idx)] = true;
					parent_idx                          = skeleton.bones[std::size_t(parent_idx)].parent_idx;
				}
			});
		}

		// trim bone-tree, removing all leaves without an aiBone (repeated)
		util::erase_if(skeleton.bones, [&](auto& bone) { return !used_bones[std::size_t(bone.idx)]; });
		skeleton.bones_by_name.clear();

		// update bone indices in bones and bones_by_name
		for(auto i = 0; i < int(skeleton.bones.size()); ++i) {
			auto& bone      = skeleton.bones[std::size_t(i)];
			bone.idx        = i;
			bone.parent_idx = bone.parent_idx == -1 ? -1 : skeleton.bones_by_name[bone.parent_name];

			skeleton.bones_by_name[bone.name] = i;
		}


		if(skeleton.bones.empty())
			return skeleton;

		auto skel_out_filename = output + "/models/" + model_name + ".mbf";
		util::to_lower_inplace(skel_out_filename);
		auto out_file = std::ofstream(skel_out_filename, std::ostream::binary | std::ostream::trunc);

		MIRRAGE_INVARIANT(out_file.is_open(), "Unable to open output file \"" << skel_out_filename << "\"!");

		// write skeleton data
		out_file.write("MBFF", 4);
		constexpr auto version = std::uint16_t(1);
		write(out_file, version);
		write(out_file, std::uint16_t(0));
		write(out_file, std::uint32_t(skeleton.bones.size()));

		write(out_file,
		      renderer::to_bone_transform(
		              glm::inverse(renderer::from_bone_transform(skeleton.bones[0].local_node_transform))));

		for(auto& bone : skeleton.bones)
			write(out_file, bone.offset);

		for(auto& bone : skeleton.bones)
			write(out_file, bone.local_node_transform);

		for(auto& bone : skeleton.bones)
			write(out_file, std::int32_t(bone.parent_idx));

		for(auto& bone : skeleton.bones) {
			write(out_file, util::Str_id(bone.name, true));
		}

		out_file.write("MBFF", 4);

		LOG(plog::debug) << "Bone count: " << skeleton.bones.size();

		return skeleton;
	}
} // namespace mirrage
