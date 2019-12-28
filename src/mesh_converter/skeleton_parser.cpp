#include "skeleton_parser.hpp"

#include "common.hpp"
#include "filesystem.hpp"

#include <mirrage/renderer/model.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/ranges.hpp>

#include <assimp/scene.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/string_cast.hpp>
#include <gsl/gsl>

#include <fstream>
#include <iostream>
#include <unordered_set>
#include <vector>


namespace mirrage {

	namespace {
		void sort_bones(std::vector<Bone_data>& bones)
		{
			if(bones.empty())
				return;

			auto added_bones = std::unordered_set<int>(bones.size());

			auto bones_out = std::vector<Bone_data>();
			bones_out.reserve(bones.size());

			auto add_bone = [&](auto& b) {
				bones_out.emplace_back(std::move(b));
				added_bones.emplace(b.idx);
				b.idx = -1;
			};
			add_bone(bones[0]);

			auto bones_to_add = std::vector<Bone_data>();

			while(bones_out.size() != bones.size()) {
				// add all nodes whos parent is already added
				for(auto& bone : bones) {
					if(bone.idx != -1 && added_bones.count(bone.parent_idx) > 0) {
						bones_to_add.emplace_back(std::move(bone));
						bone.idx = -1;
					}
				}

				std::sort(bones_to_add.begin(), bones_to_add.end(), [](auto& lhs, auto& rhs) {
					return lhs.name < rhs.name;
				});

				for(auto& b : bones_to_add)
					added_bones.emplace(b.idx);

				std::move(bones_to_add.begin(), bones_to_add.end(), std::back_inserter(bones_out));
				bones_to_add.clear();
			}

			bones = std::move(bones_out);
		}

		template <class T>
		auto read(std::istream& in)
		{
			auto v = T{};
			in.read(reinterpret_cast<char*>(&v), sizeof(T));
			return v;
		}

		void fit(std::vector<Bone_data>& bones, std::string other_skel_path, bool guess_scale)
		{
			if(other_skel_path[0] != '/')
				other_skel_path = pwd() + "/" + other_skel_path;

			try {
				auto in = std::ifstream(other_skel_path);
				in.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
				MIRRAGE_INVARIANT(in.good(), "Couldn't open existing skeleton from: " << other_skel_path);

				auto header = std::array<char, 4>();
				in.read(header.data(), header.size());
				MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'B' && header[2] == 'F'
				                          && header[3] == 'F',
				                  "Mirrage bone file '" << other_skel_path << "' corrupted (header).");

				auto version = read<std::uint16_t>(in);
				MIRRAGE_INVARIANT(version == 2,
				                  "Unsupported bone file version " << version << ". Expected 2");

				in.seekg(2, std::ios_base::cur); // skip flags

				const auto bone_count = read<std::uint32_t>(in);

				in.seekg(12 * 4, std::ios_base::cur);              // skip flags
				in.seekg(12 * 4 * bone_count, std::ios_base::cur); // skip inv. bind pose

				auto local_transforms = std::vector<renderer::Local_bone_transform>();
				local_transforms.resize(bone_count);
				in.read(reinterpret_cast<char*>(local_transforms.data()),
				        static_cast<std::streamsize>(local_transforms.size()
				                                     * sizeof(renderer::Local_bone_transform)));


				in.seekg(bone_count * (4 + 8), std::ios_base::cur); // skip bone data

				in.read(header.data(), header.size());
				MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'B' && header[2] == 'F'
				                          && header[3] == 'F',
				                  "Mirrage bone file '" << other_skel_path << "' corrupted (footer).");

				auto retarget_scale       = 0.f;
				auto retarget_scale_count = 0;

				// filter unknown bones
				auto bone_iter = bones.begin();
				for(auto i : util::range(bone_count)) {
					(void) i;

					auto len = read<std::uint32_t>(in);

					auto name = std::string();
					name.resize(len);
					in.read(name.data(), len);

					auto bone_exists =
					        std::find_if(bones.begin(), bones.end(), [&](auto& b) { return b.name == name; })
					        != bones.end();
					if(!bone_exists) {
						// insert dummy bone to keep indices in sync
						bone_iter       = bones.insert(bone_iter, Bone_data{});
						bone_iter->name = name;
						LOG(plog::info) << "Dummy Bone '" << name << "' inserted";

					} else {
						if(bone_iter == bones.end()) {
							LOG(plog::error) << "Missing bones while matching skeletons";
							std::exit(1);
						}

						// delete all bones until we've found the correct one
						while(bone_iter->name != name) {
							LOG(plog::debug) << "Delete bone '" << bone_iter->name << "'";

							// move children to our parent
							for(auto& b : bones) {
								if(b.parent_idx == bone_iter->idx) {
									b.parent_idx         = bone_iter->parent_idx;
									b.parent_name        = bone_iter->parent_name;
									b.assimp_parent_node = bone_iter->assimp_parent_node;
								}
							}
							bone_iter = bones.erase(bone_iter);
							if(bone_iter == bones.end()) {
								LOG(plog::error) << "Missing bone while matching skeletons: " << name;
								std::exit(1);
							}
						}

						if(guess_scale) {
							auto p1 = local_transforms[i].translation;
							auto p2 = bone_iter->local_node_transform.translation;

							for(int i = 0; i < 3; i++) {
								if(std::abs(p2[i]) > 1e-4f) {
									retarget_scale_count++;
									retarget_scale += std::abs(p1[i] / p2[i]);
								}
							}
						}
					}

					bone_iter++;
				}
				bone_iter = bones.erase(bone_iter, bones.end());

				if(retarget_scale_count > 0) {
					retarget_scale /= retarget_scale_count;
					for(auto& b : bones) {
						b.retarget_scale_factor = glm::vec3(retarget_scale, retarget_scale, retarget_scale);
					}
					LOG(plog::info) << "Retarget scale: " << retarget_scale;
				}

				in.read(header.data(), header.size());
				MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'B' && header[2] == 'F'
				                          && header[3] == 'F',
				                  "Mirrage bone file '" << other_skel_path << "' corrupted (V2 footer).");

				if(bone_count != bones.size()) {
					LOG(plog::error) << "Couldn't fit skeleton. Mismatching bone count (" << bones.size()
					                 << "!=" << bone_count;
					std::exit(1);
				}

			} catch(std::ifstream::failure e) {
				LOG(plog::error) << "Unable to read skeleton file for fitting: " << other_skel_path;
				std::exit(1);
			}
		}
	} // namespace

	Bone_data::Bone_data(const aiNode&                         node,
	                     const aiNode*                         assimp_parent_node,
	                     const renderer::Local_bone_transform& local_node_transform,
	                     int                                   idx,
	                     int                                   parent_idx)
	  : assimp_node(&node)
	  , assimp_parent_node(assimp_parent_node)
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
	                    const Mesh_converted_config& cfg,
	                    util::maybe<std::string>     fit_skeleton,
	                    bool                         guess_scale) -> Skeleton_data
	{
		auto skeleton = Skeleton_data{};
		// first pass over the entire scene graph and add each node as a possible bone
		auto map_node =
		        [&](const aiNode* node, const aiNode* parent_node, auto parent_idx, auto&& recurse) -> void {
			auto idx = skeleton.bones.size();
			skeleton.bones_by_name.emplace(node->mName.C_Str(), idx);
			skeleton.bones_by_assimp_node.emplace(node, idx);

			auto scale       = glm::vec3();
			auto orientation = glm::quat();
			auto translation = glm::vec3();
			auto skew        = glm::vec3();
			auto perspective = glm::vec4();
			glm::decompose(to_glm(node->mTransformation), scale, orientation, translation, skew, perspective);

			skeleton.bones.emplace_back(*node,
			                            parent_node,
			                            renderer::Local_bone_transform{orientation, translation, scale},
			                            idx,
			                            parent_idx);

			for(auto& c : gsl::span(node->mChildren, node->mNumChildren)) {
				recurse(c, node, idx, recurse);
			}
		};
		map_node(scene.mRootNode, nullptr, -1, map_node);

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
							bone_data.offset = renderer::compress_bone_transform(to_glm(bone->mOffsetMatrix)
							                                                     * inv_transform);

							skeleton.bones_by_assimp_bone.emplace(bone, node->second);

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

		if(cfg.trim_bones) {
			// trim bone-tree, removing all leaves without an aiBone (repeated)
			util::erase_if(skeleton.bones, [&](auto& bone) { return !used_bones[std::size_t(bone.idx)]; });
		}

		sort_bones(skeleton.bones);

		fit_skeleton.process([&](auto& s) { fit(skeleton.bones, s, guess_scale); });

		// update bone indices in bones and bones_by_name
		skeleton.bones_by_name.clear();
		skeleton.bones_by_assimp_bone.clear();
		skeleton.bones_by_assimp_node.clear();
		for(auto i = 0; i < int(skeleton.bones.size()); ++i) {
			auto& bone = skeleton.bones[std::size_t(i)];
			bone.idx   = i;
			if(bone.assimp_parent_node) {
				auto bone_idx_mb = util::find_maybe(skeleton.bones_by_assimp_node, bone.assimp_parent_node);
				if(bone_idx_mb.is_nothing())
					bone_idx_mb = util::find_maybe(skeleton.bones_by_name, bone.parent_name);

				bone.parent_idx = bone_idx_mb.get_or_throw(
				        "References parent ", bone.parent_name, " of ", bone.name, " doesn't exist");

			} else {
				bone.parent_idx = -1;
			}

			skeleton.bones_by_name[bone.name]               = i;
			skeleton.bones_by_assimp_bone[bone.assimp_bone] = i;
			skeleton.bones_by_assimp_node[bone.assimp_node] = i;
		}

		if(skeleton.bones.empty() || fit_skeleton.is_some())
			return skeleton;

		auto skel_out_filename = output + "/models/" + util::to_lower(model_name) + ".mbf";
		auto out_file          = std::ofstream(skel_out_filename, std::ostream::binary | std::ostream::trunc);

		MIRRAGE_INVARIANT(out_file.is_open(), "Unable to open output file \"" << skel_out_filename << "\"!");

		// write skeleton data
		out_file.write("MBFF", 4);
		constexpr auto version = std::uint16_t(2);
		write(out_file, version);
		write(out_file, std::uint16_t(std::uint16_t(cfg.skinning_type) & 0b11));
		write(out_file, std::uint32_t(skeleton.bones.size()));

		write(out_file,
		      renderer::compress_bone_transform(glm::inverse(to_glm(scene.mRootNode->mTransformation))));

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

		for(auto& bone : skeleton.bones) {
			write(out_file, bone.name);
		}

		out_file.write("MBFF", 4);

		return skeleton;
	}
} // namespace mirrage
