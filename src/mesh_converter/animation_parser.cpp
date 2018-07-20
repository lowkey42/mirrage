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

	namespace {
		auto invalid_char(char c) { return !std::isalnum(c); }

		auto to_our_behaviour(aiAnimBehaviour b)
		{
			using renderer::detail::Behaviour;
			switch(b) {
				case aiAnimBehaviour_DEFAULT: return Behaviour::node_transform;
				case aiAnimBehaviour_CONSTANT: return Behaviour::nearest;
				case aiAnimBehaviour_LINEAR: return Behaviour::extrapolate;
				case aiAnimBehaviour_REPEAT: return Behaviour::repeat;

				case _aiAnimBehaviour_Force32Bit: break;
			}

			MIRRAGE_FAIL("Invalid/Unexpected aiAnimBehaviour: " << static_cast<int>(b));
		}
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
			using namespace renderer::detail;

			auto name = std::string(anim->mName.C_Str());
			name.erase(std::remove_if(name.begin(), name.end(), invalid_char), name.end());

			auto animation_data     = Animation_data();
			animation_data.duration = static_cast<float>(anim->mDuration / anim->mTicksPerSecond);
			animation_data.bones.resize(skeleton.bones.size(), Bone_animation{});

			auto channels = gsl::span(anim->mChannels, anim->mNumChannels);
			for(auto channel : channels) {
				auto bone_idx = skeleton.bones_by_name.find(channel->mNodeName.C_Str());
				if(bone_idx == skeleton.bones_by_name.end()) {
					LOG(plog::warning) << "Couldn't find bone '" << channel->mNodeName.C_Str()
					                   << "' referenced by animation: " << anim->mName.C_Str();
					continue;
				}

				auto& per_bone_data          = animation_data.bones.at(std::size_t(bone_idx->second));
				per_bone_data.pre_behaviour  = to_our_behaviour(channel->mPreState);
				per_bone_data.post_behaviour = to_our_behaviour(channel->mPostState);

				per_bone_data.positions.reserve(channel->mNumPositionKeys);
				for(auto i = 0; i < int(channel->mNumPositionKeys); i++) {
					auto& p = channel->mPositionKeys[i];
					per_bone_data.positions.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond),
					                                     p.mValue.x,
					                                     p.mValue.y,
					                                     p.mValue.z);
				}

				per_bone_data.scales.reserve(channel->mNumScalingKeys);
				for(auto i = 0; i < int(channel->mNumScalingKeys); i++) {
					auto& p = channel->mScalingKeys[i];
					per_bone_data.scales.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond),
					                                  p.mValue.x,
					                                  p.mValue.y,
					                                  p.mValue.z);
				}

				per_bone_data.rotations.reserve(channel->mNumRotationKeys);
				for(auto i = 0; i < int(channel->mNumRotationKeys); i++) {
					auto& p = channel->mRotationKeys[i];
					per_bone_data.rotations.emplace_back(static_cast<float>(p.mTime / anim->mTicksPerSecond),
					                                     p.mValue.w,
					                                     p.mValue.x,
					                                     p.mValue.y,
					                                     p.mValue.z);
				}
			}


			auto filename = base_dir + name + ".maf";
			util::to_lower_inplace(filename);
			auto file = std::ofstream(filename);
			MIRRAGE_INVARIANT(file, "Couldn't open output animation file for: " << name);
			sf2::serialize_json(file, animation_data);
		}
	}

} // namespace mirrage
