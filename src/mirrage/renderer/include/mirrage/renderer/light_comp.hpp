#pragma once

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/utils/units.hpp>


namespace mirrage::renderer {

	class Directional_light_comp : public ecs::Component<Directional_light_comp> {
	  public:
		static constexpr const char* name() { return "Directional_light"; }
		friend void                  load_component(ecs::Deserializer& state, Directional_light_comp&);
		friend void                  save_component(ecs::Serializer& state, const Directional_light_comp&);

		using Component::Component;

		void temperature(float kelvin);
		void shadow_temperature(float kelvin);
		auto shadowcaster(bool b) noexcept { _shadowcaster = b; }
		void source_radius(util::Distance v) noexcept { _source_radius = v; }
		void intensity(float v) noexcept { _intensity = v; }
		void shadow_intensity(float v) noexcept { _shadow_intensity = v; }
		void color(util::Rgb v) noexcept { _color = v; }
		void shadow_color(util::Rgb v) noexcept { _shadow_color = v; }
		void shadowmap_id(int id) noexcept { _shadowmap_id = id; }
		void shadow_size(float v) noexcept { _shadow_size = v; }
		void shadow_near_plane(float v) noexcept { _shadow_near_plane = v; }
		void shadow_far_plane(float v) noexcept { _shadow_far_plane = v; }

		auto shadowcaster() const noexcept { return _shadowcaster; }
		auto source_radius() const noexcept { return _source_radius; }
		auto intensity() const noexcept { return _intensity; }
		auto shadow_intensity() const noexcept { return _shadow_intensity; }
		auto color() const noexcept { return _color; }
		auto shadow_color() const noexcept { return _shadow_color; }
		auto shadowmap_id() const noexcept { return _shadowmap_id; }

		void light_particles(bool b) noexcept { _light_particles = b; }
		auto light_particles() const noexcept { return _light_particles; }

		auto calc_shadowmap_view_proj(ecs::components::Transform_comp& transform) const -> glm::mat4;

		auto needs_update() { return _shadow_last_update >= _shadow_update_frequency; }
		auto on_update()
		{
			if(needs_update()) {
				_shadow_last_update = 1;
				return true;
			} else {
				_shadow_last_update++;
				return false;
			}
		}

	  private:
		util::Distance _source_radius;
		float          _intensity; // in lux
		util::Rgb      _color;
		bool           _light_particles         = false;
		float          _shadow_intensity        = 0; // in lux
		util::Rgb      _shadow_color            = {0, 0, 0};
		bool           _shadowcaster            = true;
		int            _shadowmap_id            = -1;
		float          _shadow_size             = 128;
		float          _shadow_near_plane       = 1;
		float          _shadow_far_plane        = 128;
		int            _shadow_update_frequency = 1;
		int            _shadow_last_update      = 999;
	};

	class Point_light_comp : public ecs::Component<Point_light_comp> {
	  public:
		static constexpr const char* name() { return "Point_light"; }
		friend void                  load_component(ecs::Deserializer& state, Point_light_comp&);
		friend void                  save_component(ecs::Serializer& state, const Point_light_comp&);

		using Component::Component;

		void temperature(float kelvin);
		void source_radius(util::Distance v) noexcept { _source_radius = v; }
		void intensity(float v) noexcept { _intensity = v; }
		void color(util::Rgb v) noexcept { _color = v; }

		auto source_radius() const noexcept { return _source_radius; }
		auto intensity() const noexcept { return _intensity; }
		auto color() const noexcept { return _color; }

		float calc_radius() const;

	  private:
		util::Distance _source_radius;
		float          _intensity; // in lumens
		util::Rgb      _color;
	};

	extern auto temperature_to_color(float kelvin) -> util::Rgb;


	class Shadowcaster_comp : public ecs::Stateless_tag_component<Shadowcaster_comp> {
	  public:
		static constexpr const char* name() { return "Shadowcaster"; }
		using Stateless_tag_component::Stateless_tag_component;
	};
} // namespace mirrage::renderer
