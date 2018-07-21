#pragma once

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/str_id.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <unordered_map>
#include <vector>


namespace mirrage::renderer {

	using Bone_id = std::int_fast32_t;

	/*
	* File format:
	* |   0   |   1   |   2   |  3   |
	* |   M   |   B   |   F   |  F   |
	* |    VERSION    |    RESERVED  |
	* |          BONE_COUNT          |
	*
	* |       INVERSE BIND POSE      |		mesh space -> bone space
	* * 16 * BONE_COUNT
	*
	* |        NODE TRANSFORM        |		bone space -> mesh space
	* * 16 * BONE_COUNT
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

		auto inv_bind_pose(Bone_id bone) const -> const glm::mat4&
		{
			return _inv_bind_poses[std::size_t(bone)];
		}

		auto node_transform(Bone_id bone) const -> const glm::mat4&
		{
			return _node_offset[std::size_t(bone)];
		}

		auto parent_bone(Bone_id bone) const noexcept -> util::maybe<Bone_id>
		{
			auto pid = _parent_ids[std::size_t(bone)];
			if(pid >= 0)
				return pid;
			else
				return util::nothing;
		}

	  private:
		std::vector<glm::mat4>    _inv_bind_poses;
		std::vector<glm::mat4>    _node_offset;
		std::vector<std::int32_t> _parent_ids;
		std::vector<util::Str_id> _names;

		std::unordered_map<util::Str_id, std::size_t> _bones_by_name;
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

		auto bone_transform(Bone_id, float time, Animation_key& key) const -> util::maybe<glm::mat4>;

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
