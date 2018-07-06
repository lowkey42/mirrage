#pragma once

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/entity_handle.hpp>

#include <mirrage/utils/sf2_glm.hpp>

#include <glm/gtx/quaternion.hpp>
#include <glm/vec3.hpp>


namespace mirrage::ecs::components {

	struct Transform_comp : public ecs::Component<Transform_comp> {
		static constexpr const char* name() { return "Transform"; }
		using Component::Component;

		auto direction() const noexcept -> glm::vec3;
		void direction(glm::vec3) noexcept;
		void look_at(glm::vec3) noexcept;

		void move(glm::vec3 offset) { position += offset; }
		void move_local(glm::vec3 offset);
		void rotate_local(float yaw, float pitch);

		auto to_mat4() const noexcept -> glm::mat4;

		glm::vec3 position{0, 0, 0};
		glm::quat orientation{1, 0, 0, 0};
		glm::vec3 scale{1.f, 1.f, 1.f};
	};

	sf2_structDef(Transform_comp, position, orientation, scale);

} // namespace mirrage::ecs::components
