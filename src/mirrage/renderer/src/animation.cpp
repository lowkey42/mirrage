#include <mirrage/renderer/animation.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>


namespace mirrage::renderer {

	Skeleton::Skeleton(asset::istream& file)
	{
		auto header = std::array<char, 4>();
		file.read(header.data(), header.size());
		MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'B' && header[2] == 'F' && header[3] == 'F',
		                  "Mirrage bone file '" << file.aid().str() << "' corrupted.");

		auto version = std::uint16_t(0);
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		MIRRAGE_INVARIANT(version == 1, "Unsupported bone file version " << version << ". Expected 1");

		// skip reserved
		file.read(reinterpret_cast<char*>(&version), sizeof(version));

		auto bone_count = std::uint32_t(0);
		file.read(reinterpret_cast<char*>(&bone_count), sizeof(bone_count));

		_inv_bind_poses.resize(bone_count);
		_node_offset.resize(bone_count);
		_parent_ids.resize(bone_count);
		_names.resize(bone_count);

		file.read(reinterpret_cast<char*>(_inv_bind_poses.data()), sizeof(glm::mat4) * bone_count);
		file.read(reinterpret_cast<char*>(_node_offset.data()), sizeof(glm::mat4) * bone_count);
		file.read(reinterpret_cast<char*>(_parent_ids.data()), sizeof(std::int32_t) * bone_count);
		file.read(reinterpret_cast<char*>(_names.data()), sizeof(util::Str_id) * bone_count);

		for(auto i = std::size_t(0); i < _names.size(); ++i) {
			if(_names[i])
				_bones_by_name.emplace(_names[i], i);
		}
	}


	Animation::Animation(asset::istream& file)
	{
		auto data = sf2::deserialize_json<detail::Animation_data>(file);
		_bones    = std::move(data.bones);
		_duration = std::move(data.duration);
	}

	namespace {
		template <class T>
		auto interpolate(const std::vector<detail::Timed<T>>& keyframes, float time, std::int32_t& key) -> T
		{
			// TODO: optimize, like alot; Also: contains errors calculating t
			//auto start_time = keyframes.front().time;
			//auto end_time   = keyframes.back().time;

			//time = start_time + std::fmod(time - start_time, std::abs(end_time - start_time));

			if(keyframes.size() == 1)
				return keyframes[0].value;

			key = 0;
			for(; key < int(keyframes.size()) - 1; key++) {
				if(time < keyframes[std::size_t(key) + 1].time)
					break;
			}

			auto& a = keyframes[std::size_t(key)];
			auto& b = keyframes[std::size_t(key + 1) % keyframes.size()];

			auto t_delta = b.time - a.time;
			auto t       = t_delta > 0 ? (time - a.time) / t_delta : 0.f;

			if constexpr(std::is_same_v<glm::vec3, T>) {
				return glm::mix(a.value, b.value, t);
			} else {
				return glm::normalize(glm::slerp(a.value, b.value, t));
			}
		}
	} // namespace

	auto Animation::bone_transform(Bone_id bone_idx, float time, Animation_key& key) const
	        -> util::maybe<glm::mat4>
	{
		const auto& bone_data = _bones.at(std::size_t(bone_idx));

		// TODO: aiAnimBehaviour seems to always be aiAnimBehaviour_DEFAULT, which can't be right
		//			=> loop/repeat instead for now
		if(bone_data.positions.empty() || bone_data.rotations.empty() || bone_data.scales.empty())
			return util::nothing;

		time = std::fmod(time, _duration);

		auto position = interpolate(bone_data.positions, time, key.position_key);
		auto scale    = interpolate(bone_data.scales, time, key.scale_key);
		auto rotation = interpolate(bone_data.rotations, time, key.orientation_key);

		return glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation)
		       * glm::scale(glm::mat4(1.f), scale);
	}


} // namespace mirrage::renderer
