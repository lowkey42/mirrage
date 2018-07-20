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
		template <class T>
		struct Timed {
			T     value;
			float time;

			Timed() = default;
			template <class... Args>
			Timed(float time, Args&&... args) : value(std::forward<Args>(args)...), time(time)
			{
			}
		};

		enum class Behaviour { node_transform, nearest, extrapolate, repeat };

		struct Bone_animation {
			std::vector<Timed<glm::vec3>> positions;
			std::vector<Timed<glm::vec3>> scales;
			std::vector<Timed<glm::quat>> rotations;
			Behaviour                     pre_behaviour  = Behaviour::node_transform;
			Behaviour                     post_behaviour = Behaviour::node_transform;
		};

		struct Animation_data {
			float                               duration;
			std::vector<detail::Bone_animation> bones;
		};

		template <class Reader>
		auto load(sf2::Deserializer<Reader>& reader, Timed<glm::vec3>& v)
		{
			reader.read_virtual(sf2::vmember("time", v.time),
			                    sf2::vmember("x", v.value.x),
			                    sf2::vmember("y", v.value.y),
			                    sf2::vmember("z", v.value.z));
		}
		template <class Writer>
		auto save(sf2::Serializer<Writer>& writer, const Timed<glm::vec3>& v)
		{
			writer.write_virtual(sf2::vmember("time", v.time),
			                     sf2::vmember("x", v.value.x),
			                     sf2::vmember("y", v.value.y),
			                     sf2::vmember("z", v.value.z));
		}
		template <class Reader>
		auto load(sf2::Deserializer<Reader>& reader, Timed<glm::quat>& v)
		{
			reader.read_virtual(sf2::vmember("time", v.time),
			                    sf2::vmember("x", v.value.x),
			                    sf2::vmember("y", v.value.y),
			                    sf2::vmember("z", v.value.z),
			                    sf2::vmember("w", v.value.w));
		}
		template <class Writer>
		auto save(sf2::Serializer<Writer>& writer, const Timed<glm::quat>& v)
		{
			writer.write_virtual(sf2::vmember("time", v.time),
			                     sf2::vmember("x", v.value.x),
			                     sf2::vmember("y", v.value.y),
			                     sf2::vmember("z", v.value.z),
			                     sf2::vmember("w", v.value.w));
		}

		sf2_enumDef(Behaviour, node_transform, nearest, extrapolate, repeat);
		sf2_structDef(Bone_animation, pre_behaviour, post_behaviour, positions, scales, rotations);
		sf2_structDef(Animation_data, duration, bones);

	} // namespace detail

	struct Animation_key {
		std::int32_t position_key    = 0;
		std::int32_t orientation_key = 0;
		std::int32_t scale_key       = 0;
	};

	class Animation {
	  public:
		Animation(asset::istream&);

		auto bone_transform(Bone_id, float time, Animation_key& key) const -> util::maybe<glm::mat4>;

	  private:
		std::vector<detail::Bone_animation> _bones;
		float                               _duration;
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
