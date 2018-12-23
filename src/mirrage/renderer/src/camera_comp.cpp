#include <mirrage/renderer/camera_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>


namespace mirrage::renderer {

	using namespace util::unit_literals;

	namespace {
		auto aspect_radio(const glm::vec4& viewport)
		{
			return (viewport.z - viewport.x) / (viewport.w - viewport.y);
		}
	} // namespace

	void load_component(ecs::Deserializer& state, Camera_comp& comp)
	{
		auto fov = comp._fov / 1_deg;

		state.read_virtual(sf2::vmember("fov", fov),
		                   sf2::vmember("near", comp._near),
		                   sf2::vmember("far", comp._far),
		                   sf2::vmember("dof_focus", comp._dof_focus),
		                   sf2::vmember("dof_range", comp._dof_range),
		                   sf2::vmember("dof_power", comp._dof_power));

		comp._fov = fov * 1_deg;
	}
	void save_component(ecs::Serializer& state, const Camera_comp& comp)
	{
		auto fov = comp._fov / 1_deg;

		state.write_virtual(sf2::vmember("fov", fov),
		                    sf2::vmember("near", comp._near),
		                    sf2::vmember("far", comp._far),
		                    sf2::vmember("dof_focus", comp._dof_focus),
		                    sf2::vmember("dof_range", comp._dof_range),
		                    sf2::vmember("dof_power", comp._dof_power));
	}

	auto Camera_comp::calc_projection(glm::vec4 viewport) const -> glm::mat4
	{
		auto m = glm::perspective(_fov.value(), aspect_radio(viewport), _near, _far);
		m[1][1] *= -1;
		return m;
	}

	namespace {
		auto build_inv_view(glm::vec3 position, glm::quat orientation)
		{
			auto model = glm::toMat4(orientation);
			model[3]   = glm::vec4(position, 1.f);
			return model;
		}
		auto def_projection(glm::vec4 viewport) -> glm::mat4
		{
			auto m = glm::perspective(glm::radians(50.f), aspect_radio(viewport), 0.1f, 100.f);
			m[1][1] *= -1;
			return m;
		}
	} // namespace


	Camera_state::Camera_state(glm::vec4 viewport)
	  : eye_position(0, 0, 0)
	  , viewport(viewport)
	  , inv_view(1.f)
	  , view(glm::inverse(inv_view))
	  , projection(def_projection(viewport))
	  , pure_projection(projection)
	  , view_projection(projection * view)
	  , near_plane(0.1f)
	  , far_plane(100.f)
	  , aspect_ratio((viewport.z - viewport.x) / (viewport.w - viewport.y))
	  , fov_vertical(glm::radians(50.f))
	  , fov_horizontal(2.f * std::atan(std::tan(fov_vertical / 2.f) * aspect_ratio))
	{
	}

	Camera_state::Camera_state(const Camera_comp&                     cam,
	                           const ecs::components::Transform_comp& transform,
	                           glm::vec4                              viewport)
	  : Camera_state(cam, transform.position, transform.orientation, viewport)
	{
	}
	Camera_state::Camera_state(const Camera_comp& cam,
	                           glm::vec3          position,
	                           glm::quat          orientation,
	                           glm::vec4          viewport)
	  : eye_position(position)
	  , viewport(viewport)
	  , inv_view(build_inv_view(position, orientation))
	  , view(glm::inverse(inv_view))
	  , projection(cam.calc_projection(viewport))
	  , pure_projection(projection)
	  , view_projection(projection * view)
	  , near_plane(cam.near_plane())
	  , far_plane(cam.far_plane())
	  , aspect_ratio((viewport.z - viewport.x) / (viewport.w - viewport.y))
	  , fov_vertical(cam.fov())
	  , fov_horizontal(2.f * std::atan(std::tan(fov_vertical / 2.f) * aspect_ratio))
	  , dof_focus(cam.dof_focus())
	  , dof_range(cam.dof_range())
	  , dof_power(cam.dof_power())
	{
	}

	auto Camera_state::screen_to_world(glm::vec2 screen_pos, glm::vec3 expected_pos) const noexcept
	        -> glm::vec3
	{
		auto depth = glm::project(expected_pos, glm::mat4(1), view_projection, viewport).z;
		return screen_to_world(screen_pos, depth);
	}
	auto Camera_state::screen_to_world(glm::vec2 screen_pos, float depth) const noexcept -> glm::vec3
	{
		screen_pos.y = viewport.w - screen_pos.y;

		return glm::unProject(glm::vec3(screen_pos, depth), glm::mat4(1), view_projection, viewport);
	}
	auto Camera_state::world_to_screen(glm::vec3 world_pos) const noexcept -> glm::vec2
	{
		auto sp = glm::project(world_pos, glm::mat4(1), view_projection, viewport);
		sp.y    = viewport.w - sp.y;
		return {sp.x, sp.y};
	}
} // namespace mirrage::renderer
