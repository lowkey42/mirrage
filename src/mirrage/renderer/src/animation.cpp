#include <mirrage/renderer/animation.hpp>

#include <mirrage/utils/ranges.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
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


	auto compress_bone_transform(const glm::mat4& m) -> glm::mat3x4
	{
		MIRRAGE_INVARIANT(
		        std::abs(m[0][3]) < 0.000001f && std::abs(m[1][3]) < 0.000001f
		                && std::abs(m[2][3]) < 0.000001f && std::abs(m[3][3] - 1) < 0.000001f,
		        "Last row of input into to_bone_transform is not well formed: " << glm::to_string(m));
		auto r = glm::mat3x4();
		r[0]   = glm::vec4(m[0][0], m[1][0], m[2][0], m[3][0]);
		r[1]   = glm::vec4(m[0][1], m[1][1], m[2][1], m[3][1]);
		r[2]   = glm::vec4(m[0][2], m[1][2], m[2][2], m[3][2]);
		return r;
	}


	// SKELETON
	Skeleton::Skeleton(asset::istream& file)
	{
		auto header = std::array<char, 4>();
		file.read(header.data(), header.size());
		MIRRAGE_INVARIANT(header[0] == 'M' && header[1] == 'B' && header[2] == 'F' && header[3] == 'F',
		                  "Mirrage bone file '" << file.aid().str() << "' corrupted (header).");

		auto version = read<std::uint16_t>(file);
		MIRRAGE_INVARIANT(version == 1 || version == 2,
		                  "Unsupported bone file version " << version << ". Expected 1 or 2");

		auto flags     = read<std::uint16_t>(file);
		_skinning_type = static_cast<Skinning_type>(flags & 0b11);

		auto bone_count = read<std::uint32_t>(file);

		_inv_root_transform = read<Final_bone_transform>(file);

		_inv_bind_poses.resize(bone_count);
		_node_transforms.resize(bone_count);
		_parent_ids.resize(bone_count);
		_names.resize(bone_count);

		read(file, bone_count, _inv_bind_poses);
		read(file, bone_count, _node_transforms);
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

	namespace {
		auto default_final_transform(Skinning_type st)
		{
			switch(st) {
				case Skinning_type::linear_blend_skinning: {
					auto r = Final_bone_transform();
					r.lbs  = glm::mat3x4(1);
					return r;
				}
				case Skinning_type::dual_quaternion_skinning: {
					auto r      = Final_bone_transform();
					r.dqs.dq    = glm::dual_quat_identity<float, glm::defaultp>();
					r.dqs.scale = glm::vec4(1, 1, 1, 1);
					return r;
				}
			}

			MIRRAGE_FAIL("Unknown Skinning_type: " << int(st));
		}

		auto to_bone_transform(const Local_bone_transform& t) -> Final_bone_transform
		{
			auto r = Final_bone_transform();
			r.lbs  = compress_bone_transform(glm::translate(glm::mat4(1), t.translation)
                                            * glm::mat4_cast(glm::normalize(t.orientation))
                                            * glm::scale(glm::mat4(1.f), t.scale));
			return r;
		}

		auto mul(const Final_bone_transform& rhs, const Final_bone_transform& lhs) -> Final_bone_transform
		{
			// Bone_transform is a mat4x4 with the last row cut of and transposed
			// since (A^T B^T)^T = BA => switch lhs and rhs and multiply
			const auto src_A0 = lhs.lbs[0];
			const auto src_A1 = lhs.lbs[1];
			const auto src_A2 = lhs.lbs[2];
			const auto src_B0 = rhs.lbs[0];
			const auto src_B1 = rhs.lbs[1];
			const auto src_B2 = rhs.lbs[2];

			Final_bone_transform r;
			r.lbs[0] = src_A0 * src_B0[0] + src_A1 * src_B0[1] + src_A2 * src_B0[2]
			           + glm::vec4(0, 0, 0, 1) * src_B0[3];
			r.lbs[1] = src_A0 * src_B1[0] + src_A1 * src_B1[1] + src_A2 * src_B1[2]
			           + glm::vec4(0, 0, 0, 1) * src_B1[3];
			r.lbs[2] = src_A0 * src_B2[0] + src_A1 * src_B2[1] + src_A2 * src_B2[2]
			           + glm::vec4(0, 0, 0, 1) * src_B2[3];

			return r;
		}

		void to_dualquat(Final_bone_transform& inout)
		{
			auto m2 = glm::mat4(1.f);
			m2[0]   = inout.lbs[0];
			m2[1]   = inout.lbs[1];
			m2[2]   = inout.lbs[2];
			m2[3]   = glm::vec4(0.f, 0.f, 0.f, 1.f);

			auto scale       = glm::vec3(1.f, 1.f, 1.f);
			auto orientation = glm::quat(1.f, 0.f, 0.f, 0.f);
			auto translation = glm::vec3(0.f, 0.f, 0.f);
			auto skew        = glm::vec3(0.f, 0.f, 0.f);
			auto perspective = glm::vec4(0.f, 0.f, 0.f, 0.f);
			glm::decompose(glm::transpose(m2), scale, orientation, translation, skew, perspective);

			inout.dqs.dq    = glm::dualquat(orientation, translation);
			inout.dqs.scale = glm::vec4(scale, 1);
		}
	} // namespace

	void Skeleton::to_final_transforms(gsl::span<const Local_bone_transform> in_span,
	                                   gsl::span<Final_bone_transform>       out_span) const
	{
		if(out_span.empty())
			return;

		if(in_span.size() != out_span.size()) {
			std::fill(out_span.begin(), out_span.end(), default_final_transform(_skinning_type));
			return;
		}

		// root
		out_span[0] = to_bone_transform(in_span[0]);

		for(auto [in, parent_id, out] : util::skip(1, util::join(in_span, _parent_ids, out_span))) {
			out = mul(out_span[parent_id], to_bone_transform(in));
		}

		for(auto [transform, inv_bind_pose] : util::join(out_span, _inv_bind_poses)) {
			transform = mul(mul(_inv_root_transform, transform), inv_bind_pose);
		}

		switch(_skinning_type) {
			case Skinning_type::linear_blend_skinning: break;
			case Skinning_type::dual_quaternion_skinning:
				for(auto& transform : out_span) {
					to_dualquat(transform);
				}
				break;
		}
	}


	// ANIMATION
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
		auto interpolate(gsl::span<T> keyframes, float t, std::int16_t key) -> T
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

		/// returns the index of the last element not >= value or the top/bottom if out of range
		/// performs an interpolation search starting at a given index
		/// O(1)         if the given index is already near the solution
		/// O(log log N) if the data is nearly uniformly distributed
		/// O(N)         else (worst case)
		auto interpolation_search(gsl::span<const float> container, float value, std::int16_t i)
		        -> std::int16_t
		{
			auto high = std::int16_t(container.size() - 2);
			auto low  = std::int16_t(0);
			i         = std::min(high, std::max(low, i));

			do {
				if(container[i] > value) {
					high = i - 1;
				} else if(container[i + 1] <= value) {
					low = i + 1;
				} else {
					return i;
				}

				auto new_i =
				        low + ((value - container[low]) * (high - low)) / (container[high] - container[low]);
				i = static_cast<std::int16_t>(std::min(float(high), new_i));

			} while(high > low);

			return low;
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
					case Behaviour::clamp: index = std::int16_t(times.size() - 2); return 1.f;
					case Behaviour::linear: break;
					case Behaviour::repeat: time = start_time + std::fmod(time, end_time - start_time); break;
				}
			}


			index = std::clamp(interpolation_search(times, time, index),
			                   std::int16_t(0),
			                   std::int16_t(times.size() - 2));
			return (time - times[index]) / (times[index + 1] - times[index]);
		}

	} // namespace

	auto Animation::bone_transform(Bone_id              bone_idx,
	                               float                time,
	                               Animation_key&       key,
	                               Local_bone_transform def) const -> Local_bone_transform
	{
		const auto& bone_data = _bones.at(std::size_t(bone_idx));

		if(!bone_data.positions.empty()) {
			float position_t = find_keyframe(bone_data.position_times,
			                                 time,
			                                 key.position_key,
			                                 bone_data.pre_behaviour,
			                                 bone_data.post_behaviour);

			def.translation = interpolate(bone_data.positions, position_t, key.position_key);
		}

		if(!bone_data.scales.empty()) {
			float scale_t = find_keyframe(bone_data.scale_times,
			                              time,
			                              key.scale_key,
			                              bone_data.pre_behaviour,
			                              bone_data.post_behaviour);

			def.scale = interpolate(bone_data.scales, scale_t, key.scale_key);
		}

		if(!bone_data.orientations.empty()) {
			float orientation_t = find_keyframe(bone_data.orientation_times,
			                                    time,
			                                    key.orientation_key,
			                                    bone_data.pre_behaviour,
			                                    bone_data.post_behaviour);

			def.orientation = interpolate(bone_data.orientations, orientation_t, key.orientation_key);
		}

		return def;
	}


} // namespace mirrage::renderer
