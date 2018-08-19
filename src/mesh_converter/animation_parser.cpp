#include "animation_parser.hpp"

#include "common.hpp"
#include "filesystem.hpp"
#include "skeleton_parser.hpp"

#include <mirrage/renderer/animation.hpp>
#include <mirrage/utils/log.hpp>

#include <assimp/scene.h>
#include <gsl/gsl>

#include <fstream>
#include <iostream>
#include <vector>


namespace mirrage {

	using renderer::detail::Behaviour;

	namespace {
		auto invalid_char(char c) { return !std::isalnum(c); }

		auto to_our_behaviour(aiAnimBehaviour b)
		{
			switch(b) {
				case aiAnimBehaviour_DEFAULT: [[fallthrough]];
				case aiAnimBehaviour_CONSTANT: return Behaviour::clamp;
				case aiAnimBehaviour_LINEAR: return Behaviour::linear;
				case aiAnimBehaviour_REPEAT: return Behaviour::repeat;

				case _aiAnimBehaviour_Force32Bit: break;
			}

			MIRRAGE_FAIL("Invalid/Unexpected aiAnimBehaviour: " << static_cast<int>(b));
		}

		void reserve_result_buffers(aiAnimation*            anim,
		                            std::vector<float>&     times,
		                            std::vector<glm::vec3>& positions,
		                            std::vector<glm::vec3>& scales,
		                            std::vector<glm::quat>& orientations)
		{
			auto time_count        = std::size_t(0);
			auto position_count    = std::size_t(0);
			auto scale_count       = std::size_t(0);
			auto orientation_count = std::size_t(0);
			for(auto channel : gsl::span(anim->mChannels, anim->mNumChannels)) {
				time_count = channel->mNumPositionKeys + channel->mNumScalingKeys + channel->mNumRotationKeys;
				position_count    = channel->mNumPositionKeys;
				scale_count       = channel->mNumScalingKeys;
				orientation_count = channel->mNumRotationKeys;
			}

			times.reserve(time_count);
			positions.reserve(position_count);
			scales.reserve(scale_count);
			orientations.reserve(orientation_count);
		}

		struct Bone_anim {
			Behaviour pre_behaviour  = Behaviour::clamp;
			Behaviour post_behaviour = Behaviour::clamp;

			std::size_t position_count     = 0;
			std::size_t position_times_idx = 0;
			std::size_t positions_idx      = 0;

			std::size_t scale_count     = 0;
			std::size_t scale_times_idx = 0;
			std::size_t scales_idx      = 0;

			std::size_t orientation_count     = 0;
			std::size_t orientation_times_idx = 0;
			std::size_t orientations_idx      = 0;
		};
	} // namespace

	void parse_animations(const std::string&           model_name,
	                      const std::string&           output,
	                      const aiScene&               scene,
	                      const Mesh_converted_config& cfg,
	                      const Skeleton_data&         skeleton)
	{
		if(!scene.HasAnimations())
			return;

		create_directory(output + "/animations");
		auto base_dir = output + "/animations/";

		auto animations = gsl::span(scene.mAnimations, scene.mNumAnimations);

		if(cfg.print_animations) {
			for(auto anim : animations) {
				LOG(plog::info) << "Animation for model " << model_name << ": " << anim->mName.C_Str();
			}
		}

		for(auto anim : animations) {
			auto name = std::string(anim->mName.C_Str());
			name.erase(std::remove_if(name.begin(), name.end(), invalid_char), name.end());

			auto times        = std::vector<float>();
			auto positions    = std::vector<glm::vec3>();
			auto scales       = std::vector<glm::vec3>();
			auto orientations = std::vector<glm::quat>();
			reserve_result_buffers(anim, times, positions, scales, orientations);

			auto bones = std::vector<Bone_anim>();
			bones.resize(skeleton.bones.size());

			float duration = static_cast<float>(anim->mDuration / anim->mTicksPerSecond);


			for(auto channel : gsl::span(anim->mChannels, anim->mNumChannels)) {
				auto bone_idx = skeleton.bones_by_name.find(channel->mNodeName.C_Str());
				if(bone_idx == skeleton.bones_by_name.end()) {
					LOG(plog::warning) << "Couldn't find bone '" << channel->mNodeName.C_Str()
					                   << "' referenced by animation: " << anim->mName.C_Str();
					continue;
				}

				auto& per_bone_data          = bones.at(std::size_t(bone_idx->second));
				per_bone_data.pre_behaviour  = to_our_behaviour(channel->mPreState);
				per_bone_data.post_behaviour = to_our_behaviour(channel->mPostState);

				// positions
				per_bone_data.position_count     = channel->mNumPositionKeys;
				per_bone_data.position_times_idx = times.size();
				per_bone_data.positions_idx      = positions.size();
				for(auto& p : gsl::span(channel->mPositionKeys, channel->mNumPositionKeys)) {
					times.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond));
					positions.emplace_back(p.mValue.x, p.mValue.y, p.mValue.z);
				}

				// scales
				per_bone_data.scale_count     = channel->mNumScalingKeys;
				per_bone_data.scale_times_idx = times.size();
				per_bone_data.scales_idx      = scales.size();
				for(auto& p : gsl::span(channel->mScalingKeys, channel->mNumScalingKeys)) {
					times.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond));
					scales.emplace_back(p.mValue.x, p.mValue.y, p.mValue.z);
				}

				// orientations
				per_bone_data.orientation_count     = channel->mNumRotationKeys;
				per_bone_data.orientation_times_idx = times.size();
				per_bone_data.orientations_idx      = orientations.size();
				for(auto& p : gsl::span(channel->mRotationKeys, channel->mNumRotationKeys)) {
					times.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond));
					orientations.emplace_back(p.mValue.w, p.mValue.x, p.mValue.y, p.mValue.z);
				}
			}


			auto filename = base_dir + name + ".maf";
			util::to_lower_inplace(filename);
			auto file = std::ofstream(filename);
			MIRRAGE_INVARIANT(file, "Couldn't open output animation file for: " << name);

			// write animation data
			file.write("MAFF", 4);
			constexpr auto version = std::uint16_t(1);
			write(file, version);
			write(file, std::uint16_t(0));

			write(file, float(duration));
			write(file, std::uint32_t(bones.size()));
			write(file, std::uint32_t(times.size()));
			write(file, std::uint32_t(positions.size()));
			write(file, std::uint32_t(scales.size()));
			write(file, std::uint32_t(orientations.size()));

			write(file, times);
			write(file, positions);
			write(file, scales);
			write(file, orientations);

			for(auto& bone : bones) {
				write(file, std::uint16_t(bone.pre_behaviour));
				write(file, std::uint16_t(bone.post_behaviour));

				write(file, std::uint32_t(bone.position_count));
				write(file, std::uint32_t(bone.position_times_idx));
				write(file, std::uint32_t(bone.positions_idx));

				write(file, std::uint32_t(bone.scale_count));
				write(file, std::uint32_t(bone.scale_times_idx));
				write(file, std::uint32_t(bone.scales_idx));

				write(file, std::uint32_t(bone.orientation_count));
				write(file, std::uint32_t(bone.orientation_times_idx));
				write(file, std::uint32_t(bone.orientations_idx));
			}

			file.write("MAFF", 4);
		}
	}

} // namespace mirrage
