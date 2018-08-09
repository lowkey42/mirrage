#pragma once

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/str_id.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <unordered_map>
#include <vector>


namespace mirrage::renderer {

	using Bone_id = std::int_fast32_t;

	enum class Skinning_type { linear_blend_skinning = 0b00, dual_quaternion_skinning = 0b01 };
	sf2_enumDef(Skinning_type, linear_blend_skinning, dual_quaternion_skinning);

	/// The local transform of a bone (relativ to its parent)
	struct Local_bone_transform {
		glm::quat orientation;
		glm::vec3 translation;
		glm::vec3 scale;
	};
	static_assert(sizeof(Local_bone_transform) == 4 * (4 + 3 + 3), "Local_bone_transform contains padding");

	/// compresses a bone transform into a 3x4 matrix by dropping the last row and transposing it
	extern auto compress_bone_transform(const glm::mat4&) -> glm::mat3x4;

	// The global transform of each bone, as passed to the vertex shader
	union Final_bone_transform {
		glm::mat3x4 lbs;
		struct Dqs {
			glm::dualquat dq;
			glm::vec4     scale;
		} dqs;
	};
	static_assert(sizeof(Final_bone_transform) == sizeof(glm::mat3x4), "Final_bone_transform has wrong size");
	static_assert(alignof(Final_bone_transform) == alignof(glm::mat3x4),
	              "Bone_transform has wrong alignment");


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
	*
	* |   M   |   B   |   F   |  F   |
	*/
	class Skeleton {
	  public:
		explicit Skeleton(asset::istream&);

		auto bone_count() const noexcept { return Bone_id(_inv_bind_poses.size()); }

		auto node_transform(Bone_id bone) const -> const Local_bone_transform&
		{
			return _node_transforms[std::size_t(bone)];
		}

		auto parent_bone(Bone_id bone) const noexcept -> util::maybe<Bone_id>
		{
			auto pid = _parent_ids[std::size_t(bone)];
			if(pid >= 0)
				return pid;
			else
				return util::nothing;
		}

		void to_final_transforms(gsl::span<const Local_bone_transform> in,
		                         gsl::span<Final_bone_transform>       out) const;

		auto skinning_type() const noexcept { return _skinning_type; }

	  private:
		using Bone_id_by_name = std::unordered_map<util::Str_id, std::size_t>;

		std::vector<Final_bone_transform> _inv_bind_poses;
		std::vector<Local_bone_transform> _node_transforms;
		std::vector<std::int32_t>         _parent_ids;
		std::vector<util::Str_id>         _names;
		Final_bone_transform              _inv_root_transform;
		Bone_id_by_name                   _bones_by_name;
		Skinning_type                     _skinning_type;
	};

	namespace detail {
		enum class Behaviour { clamp, linear, repeat };
	}


	struct Animation_key {
		std::int32_t position_key    = 0;
		std::int32_t scale_key       = 0;
		std::int32_t orientation_key = 0;
	};

	/*
	* File format:
	* |   0   |   1   |   2   |  3   |
	* |   M   |   A   |   F   |  F   |
	* |    VERSION    |    RESERVED  |
	* |            DURATION          |
	* |           BONE COUNT         |
	* |           TIME COUNT         |
	* |         POSITION COUNT       |
	* |          SCALE COUNT         |
	* |       ORIENTATION COUNT      |
	* |            TIMES             |
	* * TIME COUNT
	*
	* |         POSITION X           |
	* |         POSITION Y           |
	* |         POSITION Z           |
	* * POSITION COUNT
	*
	* |           SCALE X            |
	* |           SCALE Y            |
	* |           SCALE Z            |
	* * SCALE COUNT
	*
	* |        ORIENTATION X         |
	* |        ORIENTATION Y         |
	* |        ORIENTATION Z         |
	* |        ORIENTATION W         |
	* * ORIENTATION COUNT
	*
	* |PRE BEHAVIOUR | POST BEHAVIOUR|
	* |       POSITION COUNT         |
	* |  POSITION TIME START INDEX   |
	* |     POSITION START INDEX     |
	* |         SCALE COUNT          |
	* |    SCALE TIME START INDEX    |
	* |      SCALE START INDEX       |
	* |      ORIENTATION COUNT       |
	* | ORIENTATION TIME START INDEX |
	* |   ORIENTATION START INDEX    |
	* * BONE COUNT
	*
	* |   M   |   A   |   F   |  F   |
	*/
	class Animation {
	  public:
		Animation(asset::istream&);

		auto bone_transform(Bone_id, float time, Animation_key& key) const
		        -> util::maybe<Local_bone_transform>;

		auto duration() const { return _duration; }

	  private:
		struct Bone_animation {
			detail::Behaviour pre_behaviour  = detail::Behaviour::clamp;
			detail::Behaviour post_behaviour = detail::Behaviour::clamp;

			gsl::span<float>     position_times;
			gsl::span<glm::vec3> positions;
			gsl::span<float>     scale_times;
			gsl::span<glm::vec3> scales;
			gsl::span<float>     orientation_times;
			gsl::span<glm::quat> orientations;
		};


		std::vector<float>     _times;
		std::vector<glm::vec3> _positions;
		std::vector<glm::vec3> _scales;
		std::vector<glm::quat> _orientations;

		std::vector<Bone_animation> _bones;
		float                       _duration;
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
