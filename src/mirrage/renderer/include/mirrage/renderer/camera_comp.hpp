#pragma once

#include <mirrage/ecs/component.hpp>
#include <mirrage/utils/units.hpp>


namespace mirrage {
namespace ecs {namespace components {class Transform_comp;}}

namespace renderer {

	class Camera_comp : public ecs::Component<Camera_comp> {
		public:
			static constexpr const char* name() {return "Camera";}
			friend void load_component(ecs::Deserializer& state, Camera_comp&);
			friend void save_component(ecs::Serializer& state, const Camera_comp&);

			Camera_comp() = default;
			Camera_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner) {}

			auto calc_projection(glm::vec4 viewport)const -> glm::mat4;
			void priority(float p) {_priority = p;}
			auto priority()const noexcept {return _priority;}
			auto near_plane()const noexcept {return _near;}
			auto far_plane()const noexcept {return _far;}
			auto fov()const noexcept {return _fov;}

		private:
			util::Angle _fov;
			float _near = 0.2f;
			float _far = 1000.f;
			float _priority = 0.f;
	};

	struct Camera_state {
		public:
			Camera_state(const Camera_comp&, glm::vec4 viewport);
			Camera_state(const Camera_comp&, const ecs::components::Transform_comp&,
			             glm::vec4 viewport);
			Camera_state(const Camera_comp&, glm::vec3 position, glm::quat orientation,
			             glm::vec4 viewport);

			auto screen_to_world(glm::vec2 screen_pos, glm::vec3 expected_pos) const noexcept -> glm::vec3;
			auto screen_to_world(glm::vec2 screen_pos, float depth=0.99f) const noexcept -> glm::vec3;
			auto world_to_screen(glm::vec3 world_pos) const noexcept -> glm::vec2;

			glm::vec3 eye_position;
			glm::vec4 viewport;
			glm::mat4 inv_view;
			glm::mat4 view;
			glm::mat4 projection;
			glm::mat4 pure_projection;
			glm::mat4 view_projection;
			float near_plane;
			float far_plane;
			float aspect_ratio;
			util::Angle fov_vertical;
			util::Angle fov_horizontal;
	};

}
}
