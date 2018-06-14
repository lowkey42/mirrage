#include <mirrage/ecs/components/transform_comp.hpp>

#include <mirrage/utils/sf2_glm.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec3.hpp>


namespace mirrage::ecs::components {

	void load_component(ecs::Deserializer& state, Transform_comp& comp)
	{
		state.read_virtual(sf2::vmember("position", comp._position),
		                   sf2::vmember("orientation", comp._orientation),
		                   sf2::vmember("scale", comp._scale));
	}

	void save_component(ecs::Serializer& state, const Transform_comp& comp)
	{
		state.write_virtual(sf2::vmember("position", comp._position),
		                    sf2::vmember("orientation", comp._orientation),
		                    sf2::vmember("scale", comp._scale));
	}

	auto Transform_comp::direction() const noexcept -> glm::vec3
	{
		return glm::rotate(_orientation, glm::vec3(0, 0, -1));
	}
	void Transform_comp::direction(glm::vec3 dir) noexcept
	{
		_orientation = glm::rotation(glm::vec3(0, 0, -1), glm::normalize(dir));
	}

	void Transform_comp::look_at(glm::vec3 p) noexcept
	{
		_orientation = glm::quat_cast(glm::inverse(glm::lookAt(position(), p, glm::vec3(0, 1, 0))));
	}

	void Transform_comp::move_local(glm::vec3 offset) { _position += glm::rotate(_orientation, offset); }
	void Transform_comp::rotate_local(float yaw, float pitch)
	{
		auto rotation = glm::quat(glm::vec3(pitch, yaw, 0.f));

		_orientation = glm::normalize(rotation * _orientation);
	}

	auto Transform_comp::to_mat4() const noexcept -> glm::mat4
	{
		auto model = glm::toMat4(_orientation) * glm::scale(glm::mat4(1.f), _scale);
		model[3]   = glm::vec4(_position, 1.f);
		return model;
	}
} // namespace mirrage::ecs::components
