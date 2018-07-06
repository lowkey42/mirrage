#include <mirrage/ecs/components/transform_comp.hpp>

#include <mirrage/utils/sf2_glm.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec3.hpp>


namespace mirrage::ecs::components {

	auto Transform_comp::direction() const noexcept -> glm::vec3
	{
		return glm::rotate(orientation, glm::vec3(0, 0, -1));
	}
	void Transform_comp::direction(glm::vec3 dir) noexcept
	{
		orientation = glm::rotation(glm::vec3(0, 0, -1), glm::normalize(dir));
	}

	void Transform_comp::look_at(glm::vec3 p) noexcept
	{
		orientation = glm::quat_cast(glm::inverse(glm::lookAt(position, p, glm::vec3(0, 1, 0))));
	}

	void Transform_comp::move_local(glm::vec3 offset) { position += glm::rotate(orientation, offset); }
	void Transform_comp::rotate_local(float yaw, float pitch)
	{
		auto rotation = glm::quat(glm::vec3(pitch, yaw, 0.f));

		orientation = glm::normalize(rotation * orientation);
	}

	auto Transform_comp::to_mat4() const noexcept -> glm::mat4
	{
		auto model = glm::toMat4(orientation) * glm::scale(glm::mat4(1.f), scale);
		model[3]   = glm::vec4(position, 1.f);
		return model;
	}
} // namespace mirrage::ecs::components
