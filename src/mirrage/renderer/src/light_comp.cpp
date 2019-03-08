#include <mirrage/renderer/light_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>

#include <mirrage/utils/sf2_glm.hpp>


namespace mirrage::renderer {

	using namespace util::unit_literals;

	void load_component(ecs::Deserializer& state, Directional_light_comp& comp)
	{
		auto src_radius         = comp._source_radius / 1_m;
		auto temperature        = -1.f;
		auto shadow_temperature = -1.f;

		state.read_virtual(sf2::vmember("source_radius", src_radius),
		                   sf2::vmember("intensity", comp._intensity),
		                   sf2::vmember("color", comp._color),
		                   sf2::vmember("temperature", temperature),
		                   sf2::vmember("shadow_intensity", comp._shadow_intensity),
		                   sf2::vmember("shadow_color", comp._shadow_color),
		                   sf2::vmember("shadow_temperature", shadow_temperature),
		                   sf2::vmember("shadow_size", comp._shadow_size),
		                   sf2::vmember("near_plane", comp._shadow_near_plane),
		                   sf2::vmember("far_plane", comp._shadow_far_plane),
		                   sf2::vmember("update_frequency", comp._shadow_update_frequency));

		comp._source_radius = src_radius * 1_m;
		if(temperature >= 0.f) {
			comp.temperature(temperature);
		}
		if(shadow_temperature >= 0.f) {
			comp.shadow_temperature(shadow_temperature);
		}
	}

	void save_component(ecs::Serializer& state, const Directional_light_comp& comp)
	{
		state.write_virtual(sf2::vmember("source_radius", comp._source_radius / 1_m),
		                    sf2::vmember("intensity", comp._intensity),
		                    sf2::vmember("color", comp._color),
		                    sf2::vmember("shadow_intensity", comp._shadow_intensity),
		                    sf2::vmember("shadow_color", comp._shadow_color),
		                    sf2::vmember("shadow_size", comp._shadow_size),
		                    sf2::vmember("near_plane", comp._shadow_near_plane),
		                    sf2::vmember("far_plane", comp._shadow_far_plane),
		                    sf2::vmember("update_frequency", comp._shadow_update_frequency));
	}

	void Directional_light_comp::temperature(float kelvin) { _color = temperature_to_color(kelvin); }
	void Directional_light_comp::shadow_temperature(float kelvin)
	{
		_shadow_color = temperature_to_color(kelvin);
	}

	auto Directional_light_comp::calc_shadowmap_view_proj(ecs::components::Transform_comp& transform) const
	        -> glm::mat4
	{
		auto inv_view = glm::toMat4(transform.orientation);
		inv_view[3]   = glm::vec4(transform.position, 1.f);
		return glm::ortho(-_shadow_size,
		                  _shadow_size,
		                  -_shadow_size,
		                  _shadow_size,
		                  _shadow_near_plane,
		                  _shadow_far_plane)
		       * glm::inverse(inv_view);
	}

	void Point_light_comp::temperature(float kelvin) { _color = temperature_to_color(kelvin); }

	float Point_light_comp::calc_radius() const
	{
		constexpr auto cutoff = 0.01f;

		auto r = _source_radius.value() * std::sqrt(_intensity / 10000.f / cutoff);
		return util::min(20.f, r * 1.3f); // factor to compensate for coarse light volumn
	}

	void load_component(ecs::Deserializer& state, Point_light_comp& comp)
	{
		auto src_radius  = comp._source_radius / 1_m;
		auto temperature = -1.f;

		state.read_virtual(sf2::vmember("source_radius", src_radius),
		                   sf2::vmember("intensity", comp._intensity),
		                   sf2::vmember("color", comp._color),
		                   sf2::vmember("temperature", temperature));

		comp._source_radius = src_radius * 1_m;
		if(temperature >= 0.f) {
			comp.temperature(temperature);
		}
	}

	void save_component(ecs::Serializer& state, const Point_light_comp& comp)
	{
		state.write_virtual(sf2::vmember("source_radius", comp._source_radius / 1_m),
		                    sf2::vmember("intensity", comp._intensity),
		                    sf2::vmember("color", comp._color));
	}

	auto temperature_to_color(float kelvin) -> util::Rgb
	{
		// rough estimate based on http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/

		auto color = util::Rgb{};

		kelvin /= 100.f;

		// red
		if(kelvin <= 66.f) {
			color.r = 1.f;
		} else {
			color.r = glm::clamp(329.698727446f / 255.f * std::pow(kelvin - 60.f, -0.1332047592f), 0.f, 1.f);
		}

		// green
		if(kelvin <= 66.f) {
			color.g =
			        glm::clamp(99.4708025861f / 255.f * std::log(kelvin) - 161.1195681661f / 255.f, 0.f, 1.f);
		} else {
			color.g = glm::clamp(288.1221695283f / 255.f * std::pow(kelvin - 60.f, -0.0755148492f), 0.f, 1.f);
		}

		// blue
		if(kelvin >= 66.f) {
			color.b = 1.f;
		} else if(kelvin <= 19.f) {
			color.b = 0.f;
		} else {
			color.b = glm::clamp(
			        138.5177312231f / 255.f * std::log(kelvin - 10.f) - 305.0447927307f / 255.f, 0.f, 1.f);
		}

		return color;
	}
} // namespace mirrage::renderer
