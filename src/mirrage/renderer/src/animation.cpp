#include <mirrage/renderer/animation.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>


namespace mirrage::renderer {

	namespace {
		template <class T>
		auto read(asset::istream& in)
		{
			auto v = T{};
			in.read(reinterpret_cast<char*>(&v), sizeof(T));
			return v;
		}
		template <class T>
		void read(asset::istream& in, std::size_t size, std::vector<T>& out)
		{
			out.resize(size);
			in.read(reinterpret_cast<char*>(out.data()), std::streamsize(sizeof(T) * size));
		}
	} // namespace

	Skeleton::Skeleton(asset::istream& file)
	{
		auto header = std::array<char, 4>();
		file.read(header.data(), header.size());
		MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'B' && header[2] == 'F' && header[3] == 'F',
		                  "Mirrage bone file '" << file.aid().str() << "' corrupted (header).");

		auto version = read<std::uint16_t>(file);
		MIRRAGE_INVARIANT(version == 1, "Unsupported bone file version " << version << ". Expected 1");

		// skip reserved
		read<std::uint16_t>(file);

		auto bone_count = read<std::uint32_t>(file);

		_inv_bind_poses.resize(bone_count);
		_node_offset.resize(bone_count);
		_parent_ids.resize(bone_count);
		_names.resize(bone_count);

		read(file, bone_count, _inv_bind_poses);
		read(file, bone_count, _node_offset);
		read(file, bone_count, _parent_ids);
		read(file, bone_count, _names);

		for(auto i = std::size_t(0); i < _names.size(); ++i) {
			if(_names[i])
				_bones_by_name.emplace(_names[i], i);
		}

		file.read(header.data(), header.size());
		MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'B' && header[2] == 'F' && header[3] == 'F',
		                  "Mirrage bone file '" << file.aid().str() << "' corrupted (footer).");
	}


	Animation::Animation(asset::istream& file)
	{
		auto header = std::array<char, 4>();
		file.read(header.data(), header.size());
		MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'A' && header[2] == 'F' && header[3] == 'F',
		                  "Mirrage bone file '" << file.aid().str() << "' corrupted (header).");

		auto version = read<std::uint16_t>(file);
		MIRRAGE_INVARIANT(version == 1, "Unsupported bone file version " << version << ". Expected 1");

		// skip reserved
		read<std::uint16_t>(file);

		_duration              = read<float>(file);
		auto bone_count        = read<std::uint32_t>(file);
		auto time_count        = read<std::uint32_t>(file);
		auto position_count    = read<std::uint32_t>(file);
		auto scale_count       = read<std::uint32_t>(file);
		auto orientation_count = read<std::uint32_t>(file);

		read(file, time_count, _times);
		read(file, position_count, _positions);
		read(file, scale_count, _scales);
		read(file, orientation_count, _orientations);

		_bones.reserve(bone_count);
		for(auto i : util::range(bone_count)) {
			(void) i;

			auto& bone          = _bones.emplace_back();
			bone.pre_behaviour  = static_cast<detail::Behaviour>(read<std::uint16_t>(file));
			bone.post_behaviour = static_cast<detail::Behaviour>(read<std::uint16_t>(file));

			auto pos_count      = read<std::uint32_t>(file);
			auto pos_time_start = read<std::uint32_t>(file);
			auto pos_start      = read<std::uint32_t>(file);
			bone.position_times = gsl::span(&_times[pos_time_start], pos_count);
			bone.positions      = gsl::span(&_positions[pos_start], pos_count);

			auto scale_count      = read<std::uint32_t>(file);
			auto scale_time_start = read<std::uint32_t>(file);
			auto scale_start      = read<std::uint32_t>(file);
			bone.scale_times      = gsl::span(&_times[scale_time_start], scale_count);
			bone.scales           = gsl::span(&_scales[scale_start], scale_count);

			auto orientation_count      = read<std::uint32_t>(file);
			auto orientation_time_start = read<std::uint32_t>(file);
			auto orientation_start      = read<std::uint32_t>(file);
			bone.orientation_times      = gsl::span(&_times[orientation_time_start], orientation_count);
			bone.orientations           = gsl::span(&_orientations[orientation_start], orientation_count);
		}

		file.read(header.data(), header.size());
		MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'A' && header[2] == 'F' && header[3] == 'F',
		                  "Mirrage bone file '" << file.aid().str() << "' corrupted (footer).");
	}

	namespace {
		using detail::Behaviour;

		template <class T>
		auto interpolate(gsl::span<T> keyframes, float t, std::int32_t& key) -> T
		{
			auto& a = keyframes[key];
			auto& b = keyframes[(key + 1) % keyframes.size()];

			// TODO: more interesting interpolation curves

			if constexpr(std::is_same_v<glm::vec3, T>) {
				return glm::mix(a, b, t);
			} else {
				return glm::normalize(glm::slerp(a, b, t));
			}
		}

		/// returns the index of the first element not >= value or the top/bottom if out of range
		/// performs an interpolation search starting at a given index
		/// O(1)         if the given index is already near the solution
		/// O(log log N) if the data is nearly uniformly distributed
		/// O(N)         else (worst case)
		auto binary_search(gsl::span<const float> container, float value, std::int32_t i)
		{
			auto high = std::int32_t(container.size() - 2);
			auto low  = std::int32_t(0);

			do {
				if(container[i] > value) {
					high = i - 1;
				} else if(container[i + 1] <= value) {
					low = i + 1;
				} else {
					return i;
				}

				i = static_cast<std::int32_t>(
				        low + ((value - container[low]) * (high - low)) / (container[high] - container[low]));
				i = std::min(high, i);

			} while(high > low);

			return std::min(low, std::int32_t(container.size()) - 1);
		}

		/// finds the first keyframe and returns the interpolation factor
		/// Post-Conditions:
		///		0 <= index <= times.size()-2
		///		time ~= times[index] * (1-RETURN) + times[index] * RETURN
		///			depending on pre_behaviour/post_behaviour
		auto find_keyframe(gsl::span<const float> times,
		                   float                  time,
		                   std::int32_t&          index,
		                   detail::Behaviour      pre_behaviour,
		                   detail::Behaviour      post_behaviour) -> float
		{
			if(times.size() < 2) {
				index = 0;
				return 0.f;
			}

			auto start_time = times[0];
			auto end_time   = times[times.size() - 1];

			if(time < start_time) {
				switch(pre_behaviour) {
					case Behaviour::clamp: index = 0; return 0.f;
					case Behaviour::linear: break;
					case Behaviour::repeat: time = end_time + std::fmod(time, end_time - start_time); break;
				}

			} else if(time >= end_time && post_behaviour == Behaviour::repeat) {
				switch(post_behaviour) {
					case Behaviour::clamp: index = std::int32_t(times.size() - 2); return 1.f;
					case Behaviour::linear: break;
					case Behaviour::repeat: time = start_time + std::fmod(time, end_time - start_time); break;
				}
			}


			index = binary_search(times, time, index);
			return (time - times[index]) / (times[index + 1] - times[index]);
		}

	} // namespace

	auto Animation::bone_transform(Bone_id bone_idx, float time, Animation_key& key) const
	        -> util::maybe<glm::mat4>
	{
		const auto& bone_data = _bones.at(std::size_t(bone_idx));

		if(bone_data.positions.empty() || bone_data.orientations.empty() || bone_data.scales.empty())
			return util::nothing;

		float position_t = find_keyframe(bone_data.position_times,
		                                 time,
		                                 key.position_key,
		                                 bone_data.pre_behaviour,
		                                 bone_data.post_behaviour);

		float scale_t = find_keyframe(
		        bone_data.scale_times, time, key.scale_key, bone_data.pre_behaviour, bone_data.post_behaviour);

		float orientation_t = find_keyframe(bone_data.orientation_times,
		                                    time,
		                                    key.orientation_key,
		                                    bone_data.pre_behaviour,
		                                    bone_data.post_behaviour);


		auto position = interpolate(bone_data.positions, position_t, key.position_key);
		auto scale    = interpolate(bone_data.scales, scale_t, key.scale_key);
		auto rotation = interpolate(bone_data.orientations, orientation_t, key.orientation_key);


		return glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation)
		       * glm::scale(glm::mat4(1.f), scale);
	}


} // namespace mirrage::renderer
