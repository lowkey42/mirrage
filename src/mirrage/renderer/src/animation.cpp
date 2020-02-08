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
		void read(asset::istream& in, std::size_t size, T& out)
		{
			out.resize(size);
			in.read(reinterpret_cast<char*>(out.data()),
			        std::streamsize(sizeof(typename T::value_type) * size));
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
		MIRRAGE_INVARIANT(bone_count <= 128,
		                  "Number of bones per model is currently limited to 128. Found "
		                          << bone_count << " in " << file.aid().str());

		_inv_root_transform = read<Final_bone_transform>(file);

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
		auto to_bone_transform(const Local_bone_transform& t) -> Final_bone_transform
		{
			const auto q = glm::normalize(t.orientation);

			const float qxx(q.x * q.x);
			const float qyy(q.y * q.y);
			const float qzz(q.z * q.z);
			const float qxz(q.x * q.z);
			const float qxy(q.x * q.y);
			const float qyz(q.y * q.z);
			const float qwx(q.w * q.x);
			const float qwy(q.w * q.y);
			const float qwz(q.w * q.z);

			auto r  = Final_bone_transform();
			r[0][0] = 1.f - 2.f * (qyy + qzz);
			r[1][0] = 2.f * (qxy + qwz);
			r[2][0] = 2.f * (qxz - qwy);

			r[0][1] = 2.f * (qxy - qwz);
			r[1][1] = 1.f - 2.f * (qxx + qzz);
			r[2][1] = 2.f * (qyz + qwx);

			r[0][2] = 2.f * (qxz + qwy);
			r[1][2] = 2.f * (qyz - qwx);
			r[2][2] = 1.f - 2.f * (qxx + qyy);

			const auto s = glm::vec4(t.scale, 1.f);
			r[0] *= s;
			r[1] *= s;
			r[2] *= s;

			r[0][3] = t.translation[0];
			r[1][3] = t.translation[1];
			r[2][3] = t.translation[2];

			return r;
		}

		auto mul(const Final_bone_transform& rhs, const Final_bone_transform& lhs) -> Final_bone_transform
		{
			// Bone_transform is a mat4x4 with the last row cut of and transposed
			// since (A^T B^T)^T = BA => switch lhs and rhs and multiply
			const auto& src_A0 = lhs[0];
			const auto& src_A1 = lhs[1];
			const auto& src_A2 = lhs[2];
			const auto& src_B0 = rhs[0];
			const auto& src_B1 = rhs[1];
			const auto& src_B2 = rhs[2];

			Final_bone_transform r;
			r[0] = src_A0 * src_B0[0] + src_A1 * src_B0[1] + src_A2 * src_B0[2];
			r[0][3] += src_B0[3];
			r[1] = src_A0 * src_B1[0] + src_A1 * src_B1[1] + src_A2 * src_B1[2];
			r[1][3] += src_B1[3];
			r[2] = src_A0 * src_B2[0] + src_A1 * src_B2[1] + src_A2 * src_B2[2];
			r[2][3] += src_B2[3];

			return r;
		}

		auto to_dualquat(Final_bone_transform& m) -> Final_bone_transform
		{
			// decompose
			const auto translation = glm::vec3(m[0][3], m[1][3], m[2][3]);
			const auto scale =
			        glm::sqrt(glm::vec3(m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0],
			                            m[0][1] * m[0][1] + m[1][1] * m[1][1] + m[2][1] * m[2][1],
			                            m[0][2] * m[0][2] + m[1][2] * m[1][2] + m[2][2] * m[2][2]));

			const auto recp_scale = glm::vec4(1.f / scale, 1.f);
			m[0] *= recp_scale;
			m[1] *= recp_scale;
			m[2] *= recp_scale;

			auto       orientation = glm::quat();
			const auto trace       = m[0][0] + m[1][1] + m[2][2];
			if(trace > 0.f) {
				const auto r    = glm::sqrt(trace + 1.f);
				orientation.w   = 0.5f * r;
				const auto root = 0.5f / r;
				orientation.x   = root * (m[2][1] - m[1][2]);
				orientation.y   = root * (m[0][2] - m[2][0]);
				orientation.z   = root * (m[1][0] - m[0][1]);
			} else {
				constexpr int next[3] = {1, 2, 0};
				auto          i       = m[1][1] > m[0][0] ? 1 : 0;

				if(m[2][2] > m[i][i])
					i = 2;

				const auto j = next[i];
				const auto k = next[j];

				const auto r = glm::sqrt(m[i][i] - m[j][j] - m[k][k] + 1.f);

				const auto root = 0.5f / r;

				orientation[i] = 0.5f * r;
				orientation[j] = root * (m[j][i] + m[i][j]);
				orientation[k] = root * (m[k][i] + m[i][k]);
				orientation.w  = root * (m[k][j] - m[j][k]);
			}

			return {orientation.x,
			        orientation.y,
			        orientation.z,
			        orientation.w,
			        +0.5f
			                * (translation.x * orientation.w + translation.y * orientation.z
			                   - translation.z * orientation.y),
			        +0.5f
			                * (-translation.x * orientation.z + translation.y * orientation.w
			                   + translation.z * orientation.x),
			        +0.5f
			                * (translation.x * orientation.y - translation.y * orientation.x
			                   + translation.z * orientation.w),
			        -0.5f
			                * (translation.x * orientation.x + translation.y * orientation.y
			                   + translation.z * orientation.z),
			        scale.x,
			        scale.y,
			        scale.z,
			        1.f};
		}
	} // namespace

	void Skeleton::to_final_transforms(const Pose_transforms&          in_span,
	                                   gsl::span<Final_bone_transform> out_span) const
	{
		const auto size = int(in_span.size());
		auto*      out  = out_span.data();

		MIRRAGE_INVARIANT(size == int(out_span.size()) && size == int(_inv_bind_poses.size()),
		                  "Mismatching array sizes in Skeleton::to_final_transforms: "
		                          << in_span.size() << " vs. " << out_span.size() << " vs. "
		                          << _inv_bind_poses.size());

		Final_bone_transform tmp_buffer[128];

		// root
		tmp_buffer[0] = to_bone_transform(in_span[0]);

		for(auto i = 1; i < size; i++) {
			tmp_buffer[i] = mul(tmp_buffer[_parent_ids[i]], to_bone_transform(in_span[i]));
		}

		switch(_skinning_type) {
			case Skinning_type::dual_quaternion_skinning:
				for(auto i = 0; i < size; i++) {
					tmp_buffer[i] = mul(mul(_inv_root_transform, tmp_buffer[i]), _inv_bind_poses[i]);
				}
				for(auto i = 0; i < size; i++) {
					out[i] = to_dualquat(tmp_buffer[i]);
				}
				break;

			case Skinning_type::linear_blend_skinning:
			default:
				for(auto i = 0; i < size; i++) {
					out[i] = mul(mul(_inv_root_transform, tmp_buffer[i]), _inv_bind_poses[i]);
				}
				break;
		}
	}


	// ANIMATION
	Animation::Animation(asset::istream& in) : _data(load_animation(in)) {}

	namespace {
		using detail::Behaviour;

		// TODO: more interesting interpolation curves
		template <typename T>
		auto interpolate(const T& a, const T& b, float t) -> T
		{
			return glm::mix(a, b, t);
		}
		auto interpolate(const glm::quat& a, const glm::quat& b, float t) -> glm::quat
		{
			return glm::normalize(glm::slerp(a, b, t));
		}
		auto interpolate(const Local_bone_transform& a, const Local_bone_transform& b, float t)
		        -> Local_bone_transform
		{
			return {interpolate(a.orientation, b.orientation, t),
			        interpolate(a.translation, b.translation, t),
			        interpolate(a.scale, b.scale, t)};
		}

		auto linear_search(const gsl::span<const float> frame_times,
		                   const float                  max_frame_time,
		                   const float                  time,
		                   const int                    frame_idx_hint)
		{
			if(frame_times.size() <= 1)
				return std::make_tuple(0, 0, 0.f);

			const auto v = std::clamp(time, 0.f, max_frame_time);

			const auto* data = frame_times.data();
			auto        i    = std::clamp(frame_idx_hint, 1, int(frame_times.size()) - 2);

			if(data[i] >= v) { // right side is in our future => search the left
				while(data[i - 1] > v)
					i--;

			} else { // right side is in our past => search the right
				while(data[i] < v)
					i++;
			}

			const auto frame_a = i - 1;
			const auto frame_b = i;
			const auto time_a  = frame_times.at(frame_a);
			const auto time_b  = frame_times.at(frame_b);
			const auto t       = (time - time_a) / (time_b - time_a);

			return std::make_tuple(frame_a, frame_b, t);
		}

	} // namespace

	void Animation::bone_transforms(const float                     time,
	                                const float                     weight,
	                                int&                            frame_idx_hint,
	                                gsl::span<Local_bone_transform> in_out) const
	{
		const auto [frame_a, frame_b, t] =
		        linear_search(_data.frame_times, _data.duration, time, frame_idx_hint);
		const auto begin_index_a = frame_a * _data.dynamic_bone_count;
		const auto begin_index_b = frame_b * _data.dynamic_bone_count;
		const auto size          = int(in_out.size());

		frame_idx_hint = frame_b;

		MIRRAGE_INVARIANT(size == int(_data.static_transforms.size() + _data.dynamic_bone_count),
		                  "Mismatching array sizes in Animation::bone_transforms: "
		                          << size << " vs. "
		                          << (_data.static_transforms.size() + _data.dynamic_bone_count));

		auto static_offset  = 0;
		auto dynamic_offset = 0;
		for(auto i = 0; i < size; i++) {
			if(_data.is_bone_static[i]) {
				in_out[i] += weight * _data.static_transforms[static_offset++];

			} else {
				const auto offset = dynamic_offset++;
				in_out[i] += weight
				             * interpolate(_data.dynamic_transforms[begin_index_a + offset],
				                           _data.dynamic_transforms[begin_index_b + offset],
				                           t);
			}
		}
	}

} // namespace mirrage::renderer
