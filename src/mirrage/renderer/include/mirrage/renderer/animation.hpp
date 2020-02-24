#pragma once

#include <mirrage/renderer/animation_data.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/small_vector.hpp>
#include <mirrage/utils/str_id.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <unordered_map>
#include <vector>


namespace mirrage::renderer {

	/*
	* File format:
	* |   0   |   1   |   2   |  3   |
	* |   M   |   B   |   F   |  F   |
	* |    VERSION    |     FLAGS    |		flags: 2 bits skinning type, rest reserved
	* |          BONE_COUNT          |
	*
	* |    INVERSE ROOT TRANSFORM    |
	* * 12
	*
	* |       INVERSE BIND POSE      |		mesh space -> bone space
	* * 12 * BONE_COUNT
	*
	* |        NODE TRANSFORM        |		bone space -> mesh space (Local_bone_transform)
	* * 10 * BONE_COUNT
	*
	* |       BONE PARENT INDEX      |		signed; <0 => none
	* * BONE_COUNT
	*
	* |        BONE REF NAME         |		util::Str_id (empty if invalid)
	* * BONE_COUNT
	* |   M   |   B   |   F   |  F   |
	*
	* |          NAME LENGTH         |		starting from version 2
	*         NAME LENGTH bytes
	* * BONE_COUNT
	*/
	class Skeleton {
	  public:
		explicit Skeleton(asset::istream&);

		auto bone_count() const noexcept { return Bone_id(_inv_bind_poses.size()); }

		auto node_transform(Bone_id bone) const -> const Local_bone_transform&
		{
			return _node_transforms[std::size_t(bone)];
		}
		auto node_transforms() const -> gsl::span<const Local_bone_transform> { return _node_transforms; }

		auto parent_bone(Bone_id bone) const noexcept -> util::maybe<Bone_id>
		{
			auto pid = _parent_ids[std::size_t(bone)];
			if(pid >= 0)
				return pid;
			else
				return util::nothing;
		}

		void to_final_transforms(const Pose_transforms& in, gsl::span<Final_bone_transform> out) const;

		auto skinning_type() const noexcept { return _skinning_type; }

	  private:
		using Bone_id_by_name = std::unordered_map<util::Str_id, std::size_t>;

		util::small_vector<std::int32_t, 64>         _parent_ids;
		util::small_vector<Final_bone_transform, 64> _inv_bind_poses;
		Final_bone_transform                         _inv_root_transform;
		Skinning_type                                _skinning_type;
		std::vector<Local_bone_transform>            _node_transforms;
		std::vector<util::Str_id>                    _names;
		Bone_id_by_name                              _bones_by_name;
	};

	/*
	* File format: see Animation_data
	*/
	class Animation {
	  public:
		Animation(asset::istream&);

		void bone_transforms(float                           time,
		                     float                           weight,
		                     int&                            frame_idx_hint,
		                     gsl::span<Local_bone_transform> in_out) const;

		auto duration() const { return _data.duration; }

	  private:
		Animation_data _data;
	};

} // namespace mirrage::renderer

namespace mirrage::asset {
	template <>
	struct Loader<renderer::Skeleton> {
	  public:
		static auto load(istream in) -> renderer::Skeleton { return renderer::Skeleton{in}; }
		static void save(ostream, const renderer::Skeleton&)
		{
			MIRRAGE_FAIL("Save of skeletons is not supported!");
		}
	};

	template <>
	struct Loader<renderer::Animation> {
	  public:
		static auto load(istream in) -> renderer::Animation { return renderer::Animation{in}; }
		static void save(ostream, const renderer::Animation&)
		{
			MIRRAGE_FAIL("Save of animations is not supported!");
		}
	};
} // namespace mirrage::asset
