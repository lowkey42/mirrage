#pragma once

#include <mirrage/ecs/component.hpp>
#include <mirrage/utils/units.hpp>


namespace mirrage {
namespace renderer {

	class Directional_light_comp : public ecs::Component<Directional_light_comp> {
		public:
			static constexpr const char* name() {return "Directional_light";}
			friend void load_component(ecs::Deserializer& state, Directional_light_comp&);
			friend void save_component(ecs::Serializer& state, const Directional_light_comp&);

			Directional_light_comp() = default;
			Directional_light_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner) {}

			void temperature(float kelvin);
			void source_radius(util::Distance v)noexcept {_source_radius = v;}
			void intensity(float v)noexcept {_intensity = v;}
			void color(util::Rgb v)noexcept {_color = v;}
			void shadowmap_id(int id)noexcept {_shadowmap_id = id;}
			void shadow_size(float v)noexcept {_shadow_size = v;}
			void shadow_near_plane(float v)noexcept {_shadow_near_plane = v;}
			void shadow_far_plane(float v)noexcept {_shadow_far_plane = v;}

			auto source_radius()const noexcept {return _source_radius;}
			auto intensity()const noexcept {return _intensity;}
			auto color()const noexcept {return _color;}
			auto shadowmap_id()const noexcept {return _shadowmap_id;}

			auto calc_shadowmap_view_proj()const -> glm::mat4;

		private:
			util::Distance _source_radius;
			float          _intensity;
			util::Rgb      _color;
			int            _shadowmap_id = -1;
			float          _shadow_size = 128;
			float          _shadow_near_plane = 1;
			float          _shadow_far_plane  = 128;
	};

	extern auto temperature_to_color(float kelvin) -> util::Rgb;


	class Shadowcaster_comp : public ecs::Component<Shadowcaster_comp> {
		public:
			static constexpr const char* name() {return "Shadowcaster";}
			// friend void load_component(ecs::Deserializer& state, Directional_light_comp&);
			// friend void save_component(ecs::Serializer& state, const Directional_light_comp&);

			Shadowcaster_comp() = default;
			Shadowcaster_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner) {}
	};

}
}
