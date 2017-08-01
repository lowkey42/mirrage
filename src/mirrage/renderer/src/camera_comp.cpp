#include <mirrage/renderer/camera_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>


namespace mirrage {
namespace renderer {

	using namespace util::unit_literals;

	namespace {
		auto aspect_radio(const glm::vec4& viewport) {
			return (viewport.z-viewport.x) / (viewport.w-viewport.y);
		}
	}

	void load_component(ecs::Deserializer& state, Camera_comp& comp) {
		auto fov = comp._fov / 1_deg;

		state.read_virtual(
			sf2::vmember("fov", fov),
			sf2::vmember("near", comp._near),
			sf2::vmember("far", comp._far)
		);

		comp._fov = fov * 1_deg;
	}
	void save_component(ecs::Serializer& state, const Camera_comp& comp) {
		auto fov = comp._fov / 1_deg;

		state.write_virtual(
			sf2::vmember("fov", fov),
			sf2::vmember("near", comp._near),
			sf2::vmember("far", comp._far)
		);
	}

	auto Camera_comp::calc_projection(glm::vec4 viewport)const -> glm::mat4 {
		auto m = glm::perspective(_fov.value(), aspect_radio(viewport), _near, _far);
		m[1][1] *= -1;
		return m;
	}

	namespace {
		auto build_inv_view(glm::vec3 position, glm::quat orientation) {
			auto model = glm::toMat4(orientation);
			model[3] = glm::vec4(position, 1.f);
			return model;
		}
	}

	Camera_state::Camera_state(const Camera_comp& cam, glm::vec4 viewport)
	    : Camera_state(cam, cam.owner().get<ecs::components::Transform_comp>().get_or_throw(), viewport) {
	}
	Camera_state::Camera_state(const Camera_comp& cam, const ecs::components::Transform_comp& transform,
	                           glm::vec4 viewport)
	    : Camera_state(cam, transform.position(), transform.orientation(), viewport) {
	}
	Camera_state::Camera_state(const Camera_comp& cam, glm::vec3 position, glm::quat orientation,
	                           glm::vec4 viewport)
	    : eye_position(position)
	    , viewport(viewport)
	    , inv_view(build_inv_view(position, orientation))
	    , view(glm::inverse(inv_view))
	    , projection(cam.calc_projection(viewport))
	    , pure_projection(projection)
	    , view_projection(projection * view)
	    , near_plane(cam.near_plane())
	    , far_plane(cam.far_plane())
	    , aspect_ratio((viewport.z-viewport.x) / (viewport.w-viewport.y))
	    , fov_vertical(cam.fov())
	    , fov_horizontal(2.f * std::atan(std::tan(fov_vertical/2.f) * aspect_ratio)) {
	}

	auto Camera_state::screen_to_world(glm::vec2 screen_pos, glm::vec3 expected_pos) const noexcept -> glm::vec3 {
		auto depth = glm::project(expected_pos, glm::mat4(1), view_projection, viewport).z;
		return screen_to_world(screen_pos, depth);
	}
	auto Camera_state::screen_to_world(glm::vec2 screen_pos, float depth) const noexcept -> glm::vec3 {
		screen_pos.y = viewport.w-screen_pos.y;

		return glm::unProject(glm::vec3(screen_pos,depth), glm::mat4(1),
		                      view_projection, viewport);
	}
	auto Camera_state::world_to_screen(glm::vec3 world_pos) const noexcept -> glm::vec2 {
		auto sp = glm::project(world_pos, glm::mat4(1), view_projection, viewport);
		sp.y = viewport.w-sp.y;
		return {sp.x, sp.y};
	}


}
}
