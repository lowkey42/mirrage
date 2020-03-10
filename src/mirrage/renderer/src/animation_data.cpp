#include <mirrage/renderer/animation_data.hpp>

#include <mirrage/utils/ranges.hpp>


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
		void read(asset::istream& in, std::size_t size, T& out)
		{
			out.resize(size);
			in.read(reinterpret_cast<char*>(out.data()),
			        std::streamsize(sizeof(typename T::value_type) * size));
		}

		template <typename T>
		void write(std::ostream& out, const T& value)
		{
			static_assert(!std::is_pointer<T>::value,
			              "T is a pointer. That is DEFINITLY not what you wanted!");
			out.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		template <typename C>
		void write_container(std::ostream& out, const C& value)
		{
			static_assert(!std::is_pointer<typename C::value_type>::value,
			              "C::value_type is a pointer. That is DEFINITLY not what you wanted!");
			out.write(reinterpret_cast<const char*>(value.data()),
			          value.size() * sizeof(typename C::value_type));
		}

		[[maybe_unused]] auto load_animation_body_v1(asset::istream& in) -> detail::Animation_data_v1
		{
			auto result = detail::Animation_data_v1{};
			// skip reserved
			read<std::uint16_t>(in);

			result.duration        = read<float>(in);
			auto bone_count        = read<std::uint32_t>(in);
			auto time_count        = read<std::uint32_t>(in);
			auto position_count    = read<std::uint32_t>(in);
			auto scale_count       = read<std::uint32_t>(in);
			auto orientation_count = read<std::uint32_t>(in);

			read(in, time_count, result.times);
			read(in, position_count, result.positions);
			read(in, scale_count, result.scales);
			read(in, orientation_count, result.orientations);

			result.bones.reserve(bone_count);
			for(auto i : util::range(bone_count)) {
				(void) i;

				auto& bone          = result.bones.emplace_back();
				bone.pre_behaviour  = static_cast<detail::Behaviour>(read<std::uint16_t>(in));
				bone.post_behaviour = static_cast<detail::Behaviour>(read<std::uint16_t>(in));

				auto pos_count      = read<std::uint32_t>(in);
				auto pos_time_start = read<std::uint32_t>(in);
				auto pos_start      = read<std::uint32_t>(in);
				bone.position_times = gsl::span(&result.times[pos_time_start], pos_count);
				bone.positions      = gsl::span(&result.positions[pos_start], pos_count);

				auto scale_count      = read<std::uint32_t>(in);
				auto scale_time_start = read<std::uint32_t>(in);
				auto scale_start      = read<std::uint32_t>(in);
				bone.scale_times      = gsl::span(&result.times[scale_time_start], scale_count);
				bone.scales           = gsl::span(&result.scales[scale_start], scale_count);

				auto orientation_count      = read<std::uint32_t>(in);
				auto orientation_time_start = read<std::uint32_t>(in);
				auto orientation_start      = read<std::uint32_t>(in);
				bone.orientation_times = gsl::span(&result.times[orientation_time_start], orientation_count);
				bone.orientations = gsl::span(&result.orientations[orientation_start], orientation_count);
			}

			auto header = std::array<char, 4>();
			in.read(header.data(), header.size());
			MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'A' && header[2] == 'F' && header[3] == 'F',
			                  "Mirrage bone file '" << in.aid().str() << "' corrupted (footer).");

			return result;
		}

		[[maybe_unused]] auto interpolation_search(const gsl::span<const float> container,
		                                           const float                  value,
		                                           const std::int16_t           i) -> std::int16_t
		{
			auto high  = int(container.size() - 2);
			auto low   = int(0);
			auto index = std::clamp(int(i), low, high);

			do {
				if(container[index] > value) {
					high = index - 1;
				} else if(container[index + 1] <= value) {
					low = index + 1;
				} else {
					return std::int16_t(index);
				}

				auto new_i =
				        low + ((value - container[low]) * (high - low)) / (container[high] - container[low]);
				index = static_cast<int>(std::min(float(high), new_i));

			} while(high > low);

			return std::int16_t(low);
		}

		/// finds the first keyframe and returns the interpolation factor
		/// Post-Conditions:
		///		0 <= index <= times.size()-2
		///		time ~= times[index] * (1-RETURN) + times[index] * RETURN
		///			depending on pre_behaviour/post_behaviour
		auto find_keyframe(gsl::span<const float> times,
		                   float                  time,
		                   std::int16_t&          index,
		                   detail::Behaviour      pre_behaviour,
		                   detail::Behaviour      post_behaviour) -> float
		{
			using detail::Behaviour;

			if(times.size() <= 1) {
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
					case Behaviour::clamp: index = std::int16_t(times.size() - 2); return 1.f;
					case Behaviour::linear: break;
					case Behaviour::repeat: time = start_time + std::fmod(time, end_time - start_time); break;
				}
			}

			index = std::clamp(std::int16_t(std::distance(times.begin(),
			                                              std::lower_bound(times.begin(), times.end(), time))
			                                - 1),
			                   std::int16_t(0),
			                   std::int16_t(times.size() - 2));

			index = std::clamp(
			        interpolation_search(times, time, 0), std::int16_t(0), std::int16_t(times.size() - 2));

			return (time - times[index]) / (times[index + 1] - times[index]);
		}

		template <class T>
		auto interpolate(gsl::span<T> keyframes, float t, std::int16_t key) -> T
		{
			auto& a = keyframes[key];
			auto& b = keyframes[(key + 1) % keyframes.size()];

			if constexpr(std::is_same_v<glm::vec3, T>) {
				return glm::mix(a, b, t);
			} else {
				return glm::normalize(glm::slerp(a, b, t));
			}
		}

		auto sample_v1(const detail::Animation_data_v1& anim,
		               Bone_id                          bone_idx,
		               float                            time,
		               const Local_bone_transform&      def) -> Local_bone_transform
		{
			auto ret = Local_bone_transform{};

			const auto& bone_data = anim.bones.at(std::size_t(bone_idx));

			auto position_key    = std::int16_t{0};
			auto scale_key       = std::int16_t{0};
			auto orientation_key = std::int16_t{0};

			if(!bone_data.positions.empty()) {
				float position_t = find_keyframe(bone_data.position_times,
				                                 time,
				                                 position_key,
				                                 bone_data.pre_behaviour,
				                                 bone_data.post_behaviour);

				ret.translation = interpolate(bone_data.positions, position_t, position_key);
			} else {
				ret.translation = def.translation;
			}

			if(!bone_data.scales.empty()) {
				float scale_t = find_keyframe(bone_data.scale_times,
				                              time,
				                              scale_key,
				                              bone_data.pre_behaviour,
				                              bone_data.post_behaviour);

				ret.scale = interpolate(bone_data.scales, scale_t, scale_key);
			} else {
				ret.scale = def.scale;
			}

			if(!bone_data.orientations.empty()) {
				float orientation_t = find_keyframe(bone_data.orientation_times,
				                                    time,
				                                    orientation_key,
				                                    bone_data.pre_behaviour,
				                                    bone_data.post_behaviour);

				ret.orientation = interpolate(bone_data.orientations, orientation_t, orientation_key);
			} else {
				ret.orientation = def.orientation;
			}

			return ret;
		}
	} // namespace

	auto load_animation(asset::istream& in) -> Animation_data
	{
		auto header = std::array<char, 4>();
		in.read(header.data(), header.size());
		MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'A' && header[2] == 'F' && header[3] == 'F',
		                  "Mirrage animation file '" << in.aid().str() << "' corrupted (header).");

		const auto version = read<std::uint16_t>(in);

		if(version == 2) {
			auto result = detail::Animation_data_v2{};
			// skip reserved
			read<std::uint16_t>(in);

			result.duration = read<float>(in);

			const auto bone_count_static  = read<std::uint32_t>(in);
			const auto bone_count_dynamic = read<std::uint32_t>(in);
			const auto keyframe_count     = read<std::uint32_t>(in);

			result.dynamic_bone_count = bone_count_dynamic;

			for(auto i = 0; i < int(result.is_bone_static.size()); i += 8) {
				auto bits = std::istream::char_type();
				in.get(bits);
				for(auto j = 0; j < 8; j++) {
					result.is_bone_static[i + j] = (bits >> (7 - j)) & 1;
				}
			}

			read(in, keyframe_count, result.frame_times);
			read(in, bone_count_dynamic * keyframe_count, result.dynamic_transforms);
			read(in, bone_count_static, result.static_transforms);

			in.read(header.data(), header.size());

			MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'A' && header[2] == 'F' && header[3] == 'F',
			                  "Mirrage bone file '" << in.aid().str() << "' corrupted (footer).");

			return result;

		} else {
			MIRRAGE_FAIL("Unsupported animation file version " << version << " in animation file '"
			                                                   << in.aid().str() << "'. Expected 2");
		}
	}

	void save_animation(std::ostream& out, const Animation_data& data)
	{
		out.write("MAFF", 4);
		write(out, std::uint16_t(2)); // version
		write(out, std::uint16_t(0)); // reserved

		write(out, data.duration);
		write(out, std::uint32_t(data.static_transforms.size()));
		write(out, std::uint32_t(data.dynamic_bone_count));
		write(out, std::uint32_t(data.frame_times.size()));

		for(auto i = 0; i < int(data.is_bone_static.size()); i += 8) {
			auto bits = std::uint8_t(0);
			for(auto j = 0; j < 8; j++) {
				bits = (bits << 1) | (data.is_bone_static[i + j] ? 1 : 0);
			}
			out.put(bits);
		}

		write_container(out, data.frame_times);
		write_container(out, data.dynamic_transforms);
		write_container(out, data.static_transforms);

		out.write("MAFF", 4);
	}

	auto resample_animation(detail::Animation_data_v1             in,
	                        gsl::span<const Local_bone_transform> default_transforms)
	        -> detail::Animation_data_v2
	{
		MIRRAGE_INVARIANT(std::size_t(default_transforms.size()) == in.bones.size(),
		                  "Incompatible default transforms in resample_animation: "
		                          << default_transforms.size() << "!=" << in.bones.size());

		auto out = detail::Animation_data_v2{};

		// find unique frame times
		out.frame_times = in.times;
		std::sort(out.frame_times.begin(), out.frame_times.end());
		out.frame_times.erase(
		        std::unique(out.frame_times.begin(),
		                    out.frame_times.end(),
		                    [](auto lhs, auto rhs) { return std::abs(rhs - lhs) < 1.f / 10.f; }),
		        out.frame_times.end());
		out.frame_times.shrink_to_fit();
		out.duration = util::min(out.frame_times.back(), in.duration);


		// find static bones
		for(auto&& [index, bone] : util::with_index(in.bones)) {
			auto max_bone_frames =
			        util::max(bone.positions.size(), bone.scales.size(), bone.orientations.size());
			out.is_bone_static[index] = max_bone_frames <= 1;

			if(out.is_bone_static[index]) {
				out.static_transforms.emplace_back(sample_v1(in, index, 0.f, default_transforms[index]));
			}
		}

		out.dynamic_bone_count = static_cast<Bone_id>(in.bones.size() - out.static_transforms.size());
		out.dynamic_transforms.reserve(out.frame_times.size() * out.dynamic_bone_count);

		// take sample at each keyframe
		for(auto time : out.frame_times) {
			for(auto&& [bone_index, bone] : util::with_index(in.bones)) {
				if(~out.is_bone_static[bone_index]) {
					out.dynamic_transforms.emplace_back(
					        sample_v1(in, bone_index, time, default_transforms[bone_index]));
				}
			}
		}

		MIRRAGE_INVARIANT((out.frame_times.size() * out.dynamic_bone_count) == out.dynamic_transforms.size(),
		                  "Size mismatch");

		LOG(plog::info) << "Resampled animation: " << out.frame_times.size() << " frames with "
		                << out.dynamic_bone_count << " dynamic bones, duration " << out.duration
		                << ", keyframes " << out.frame_times.front() << " - " << out.frame_times.back();

		return out;
	}

} // namespace mirrage::renderer
