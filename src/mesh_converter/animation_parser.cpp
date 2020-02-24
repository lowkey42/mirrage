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
				time_count +=
				        channel->mNumPositionKeys + channel->mNumScalingKeys + channel->mNumRotationKeys;
				position_count += channel->mNumPositionKeys;
				scale_count += channel->mNumScalingKeys;
				orientation_count += channel->mNumRotationKeys;
			}

			times.reserve(time_count);
			positions.reserve(position_count);
			scales.reserve(scale_count);
			orientations.reserve(orientation_count);
		}
	} // namespace

	void parse_animations(const std::string&              model_name,
	                      const util::maybe<std::string>& name,
	                      const std::string&              output,
	                      const aiScene&                  scene,
	                      const Mesh_converted_config&    cfg,
	                      const Skeleton_data&            skeleton)
	{
		if(!scene.HasAnimations())
			return;

		create_directory(output + "/animations");
		auto base_dir = output + "/animations/";

		auto animations = gsl::span(scene.mAnimations, scene.mNumAnimations);
		for(auto anim : animations) {
			auto animation = renderer::detail::Animation_data_v1();

			auto anim_name = std::string(anim->mName.C_Str());
			anim_name.erase(std::remove_if(anim_name.begin(), anim_name.end(), invalid_char),
			                anim_name.end());

			auto& times        = animation.times;
			auto& positions    = animation.positions;
			auto& scales       = animation.scales;
			auto& orientations = animation.orientations;
			reserve_result_buffers(anim, times, positions, scales, orientations);

			auto& bones = animation.bones;
			bones.resize(skeleton.bones.size());

			animation.duration = static_cast<float>(anim->mDuration / anim->mTicksPerSecond);

			for(auto channel : gsl::span(anim->mChannels, anim->mNumChannels)) {
				auto bone_name = std::string(channel->mNodeName.C_Str());
				auto bone_idx  = skeleton.bones_by_name.find(bone_name);
				if(auto i = bone_name.find("_$AssimpFbx$"); i != std::string::npos) {
					bone_name = bone_name.substr(0, i);
					bone_idx  = skeleton.bones_by_name.find(bone_name);
				}

				if(bone_idx == skeleton.bones_by_name.end()) {
					LOG(plog::warning) << "Couldn't find bone '" << channel->mNodeName.C_Str()
					                   << "' referenced by animation: " << anim->mName.C_Str();
					continue;
				}

				const auto retarget_scale =
				        skeleton.bones.at(std::size_t(bone_idx->second)).retarget_scale_factor;

				auto& per_bone_data          = bones.at(std::size_t(bone_idx->second));
				per_bone_data.pre_behaviour  = to_our_behaviour(channel->mPreState);
				per_bone_data.post_behaviour = to_our_behaviour(channel->mPostState);


				// positions
				if(channel->mNumPositionKeys > per_bone_data.positions.size() && cfg.animate_translation) {
					const auto time_idx     = times.size();
					const auto position_idx = positions.size();
					const auto count        = long(channel->mNumPositionKeys);
					for(auto& p : gsl::span(channel->mPositionKeys, count)) {
						times.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond));
						positions.emplace_back(glm::vec3(p.mValue.x, p.mValue.y, p.mValue.z)
						                       * retarget_scale);
					}
					per_bone_data.position_times = {times.data() + time_idx, count};
					per_bone_data.positions      = {positions.data() + position_idx, count};
				}

				// scales
				if(channel->mNumScalingKeys > per_bone_data.scales.size() && cfg.animate_scale) {
					const auto time_idx  = times.size();
					const auto scale_idx = scales.size();
					const auto count     = long(channel->mNumScalingKeys);
					for(auto& p : gsl::span(channel->mScalingKeys, count)) {
						times.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond));
						scales.emplace_back(p.mValue.x, p.mValue.y, p.mValue.z);
					}
					per_bone_data.scale_times = {times.data() + time_idx, count};
					per_bone_data.scales      = {scales.data() + scale_idx, count};
				}

				// orientations
				if(channel->mNumRotationKeys > per_bone_data.orientations.size() && cfg.animate_orientation) {
					const auto time_idx        = times.size();
					const auto orientation_idx = orientations.size();
					const auto count           = long(channel->mNumRotationKeys);
					for(auto& p : gsl::span(channel->mRotationKeys, count)) {
						times.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond));
						orientations.emplace_back(p.mValue.w, p.mValue.x, p.mValue.y, p.mValue.z);
					}
					per_bone_data.orientation_times = {times.data() + time_idx, count};
					per_bone_data.orientations      = {orientations.data() + orientation_idx, count};
				}
			}


			auto filename = base_dir + util::to_lower(model_name + "_" + anim_name) + ".maf";
			if(cfg.only_animations && animations.size() == 1 && name.is_some())
				filename = base_dir + name.get_or_throw() + ".maf";

			auto file = std::ofstream(filename);
			MIRRAGE_INVARIANT(file, "Couldn't open output animation file for: " << anim_name);

			auto default_transforms = std::vector<renderer::Local_bone_transform>();
			for(auto& bone : skeleton.bones) {
				default_transforms.emplace_back(bone.local_node_transform);
			}
			renderer::save_animation(file, resample_animation(animation, default_transforms));
		}
	}

} // namespace mirrage
