#pragma once

#include <mirrage/asset/stream.hpp>
#include <mirrage/utils/small_vector.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <gsl/gsl>

#include <bitset>
#include <unordered_map>
#include <vector>


namespace mirrage::renderer {

	using Bone_id = std::int_fast16_t;

	enum class Skinning_type { linear_blend_skinning = 0b00, dual_quaternion_skinning = 0b01 };
	sf2_enumDef(Skinning_type, linear_blend_skinning, dual_quaternion_skinning);

	/// The local transform of a bone (relativ to its parent)
	struct Local_bone_transform {
		glm::quat orientation;
		glm::vec3 translation;
		glm::vec3 scale;

		friend auto operator*(const Local_bone_transform& lhs, float rhs) -> Local_bone_transform
		{
			return {lhs.orientation * rhs, lhs.translation * rhs, lhs.scale * rhs};
		}
		friend auto operator*(float lhs, const Local_bone_transform& rhs) -> Local_bone_transform
		{
			return {rhs.orientation * lhs, rhs.translation * lhs, rhs.scale * lhs};
		}
		friend auto operator+(const Local_bone_transform& lhs, const Local_bone_transform& rhs)
		        -> Local_bone_transform
		{
			return {lhs.orientation + rhs.orientation,
			        lhs.translation + rhs.translation,
			        lhs.scale + rhs.scale};
		}
		auto operator+=(const Local_bone_transform& rhs)
		{
			orientation += rhs.orientation;
			translation += rhs.translation;
			scale += rhs.scale;
		}
	};
	static_assert(sizeof(Local_bone_transform) == 4 * (4 + 3 + 3), "Local_bone_transform contains padding");
	static_assert(alignof(Local_bone_transform) == alignof(float), "Local_bone_transform is misaligned");

	using Pose_transforms = util::small_vector<Local_bone_transform, 64>;

	/// compresses a bone transform into a 3x4 matrix by dropping the last row and transposing it
	extern auto compress_bone_transform(const glm::mat4&) -> glm::mat3x4;

	// The global transform of each bone, as passed to the vertex shader
	using Final_bone_transform = glm::mat3x4;


	namespace detail {
		enum class Behaviour { clamp, linear, repeat };

		/*
		* File format:
		* |   0   |   1   |   2   |  3   |
		* |   M   |   A   |   F   |  F   |
		* |    VERSION    |    RESERVED  | VERSION == 2
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
		struct Animation_data_v1 {
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


			std::vector<float>     times;
			std::vector<glm::vec3> positions;
			std::vector<glm::vec3> scales;
			std::vector<glm::quat> orientations;

			std::vector<Bone_animation> bones;
			float                       duration;
		};


		/*
		* File format:
		* |   0   |   1   |   2   |  3   |
		* |   M   |   A   |   F   |  F   |
		* |    VERSION    |    RESERVED  | VERSION == 2
		* |            DURATION          | in seconds as float
		* |       STATIC BONE COUNT      |
		* |       DYNAMIC BONE COUNT     |
		* |           TIME COUNT         |
		*
		* | ## IS_BONE_STATIC BIT MASK ##| one bit per bone (1=static, 0=dynamic)
		* | ## IS_BONE_STATIC BIT MASK ##| padded to 128 bits
		* | ## IS_BONE_STATIC BIT MASK ##|
		* | ## IS_BONE_STATIC BIT MASK ##|
		*
		* |         KEYFRAME TIME        | in seconds as float
		* * TIME COUNT
		*
		* |        ORIENTATION X         |
		* |        ORIENTATION Y         |
		* |        ORIENTATION Z         |
		* |        ORIENTATION W         |
		* |         POSITION X           |
		* |         POSITION Y           |
		* |         POSITION Z           |
		* |           SCALE X            |
		* |           SCALE Y            |
		* |           SCALE Z            |
		* * DYNAMIC BONE COUNT
		* * TIME COUNT
		*
		* |        ORIENTATION X         |
		* |        ORIENTATION Y         |
		* |        ORIENTATION Z         |
		* |        ORIENTATION W         |
		* |         POSITION X           |
		* |         POSITION Y           |
		* |         POSITION Z           |
		* |           SCALE X            |
		* |           SCALE Y            |
		* |           SCALE Z            |
		* * STATIC BONE COUNT
		* |   M   |   A   |   F   |  F   |
		*/
		struct Animation_data_v2 {
			std::vector<float> frame_times;
			float              duration;
			Bone_id            dynamic_bone_count;

			std::bitset<128> is_bone_static; //< bone transforms not affected by this animation

			/// local transforms of each frame and bones arranged as:
			///     x frame
			///	        x _dynamic_bone_count for each bone with _is_bone_static[]==false (in order)
			std::vector<Local_bone_transform>            dynamic_transforms;
			util::small_vector<Local_bone_transform, 64> static_transforms;
		};
	} // namespace detail

	using Animation_data = detail::Animation_data_v2;

	extern auto load_animation(asset::istream&) -> Animation_data;
	extern void save_animation(std::ostream&, const Animation_data&);

	extern auto resample_animation(detail::Animation_data_v1,
	                               gsl::span<const Local_bone_transform> default_transforms)
	        -> detail::Animation_data_v2;

} // namespace mirrage::renderer
