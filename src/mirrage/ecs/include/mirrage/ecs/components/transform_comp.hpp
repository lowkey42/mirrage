#pragma once

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/entity_handle.hpp>

#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>


namespace mirrage {
namespace ecs {
namespace components {

	class Transform_comp : public ecs::Component<Transform_comp> {
		public:
			static constexpr const char* name() {return "Transform";}
			friend void load_component(ecs::Deserializer& state, Transform_comp&);
			friend void save_component(ecs::Serializer& state, const Transform_comp&);

			Transform_comp() = default;
			Transform_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner) {}

			auto position()   const noexcept {return _position;}
			auto orientation()const noexcept {return _orientation;}
			auto scale()      const noexcept {return _scale;}

			void position   (glm::vec3 v)noexcept {_position=v;}
			void orientation(glm::quat v)noexcept {_orientation=v;}
			void scale      (float v)    noexcept {_scale=glm::vec3(v,v,v);}
			void scale      (glm::vec3 v)noexcept {_scale=v;}

			auto direction()const noexcept -> glm::vec3;
			void direction(glm::vec3)noexcept;
			void look_at(glm::vec3)noexcept;

			void move(glm::vec3 offset) {
				_position += offset;
			}
			void move_local(glm::vec3 offset);
			void rotate_local(float yaw, float pitch);

			auto to_mat4()const noexcept -> glm::mat4;

		private:
			glm::vec3 _position {0,0,0};
			glm::quat _orientation {1,0,0,0};
			glm::vec3 _scale {1.f, 1.f, 1.f};
	};

}
}
}
