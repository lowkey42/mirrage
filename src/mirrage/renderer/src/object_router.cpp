#include <mirrage/renderer/object_router.hpp>

namespace mirrage::renderer {

	namespace {
		auto norm_plane(glm::vec4 p) { return p / glm::length(glm::vec3(p.x, p.y, p.z)); }
		auto extract_planes(const glm::mat4& cam_view_proj) -> detail::Frustum_planes
		{
			return {
			        norm_plane(row(cam_view_proj, 3) + row(cam_view_proj, 0)), // left
			        norm_plane(row(cam_view_proj, 3) - row(cam_view_proj, 0)), // right
			        norm_plane(row(cam_view_proj, 3) - row(cam_view_proj, 1)), // top
			        norm_plane(row(cam_view_proj, 3) + row(cam_view_proj, 1)), // bottom
			        norm_plane(row(cam_view_proj, 3) + row(cam_view_proj, 2)), // near
			        norm_plane(row(cam_view_proj, 3) - row(cam_view_proj, 2))  // far
			};
		}
	} // namespace

	Object_router_base::~Object_router_base() = default;

	auto Object_router_base::add_viewer(const glm::mat4& view_proj, bool is_camera) -> Culling_mask
	{
		_frustums.emplace_back(extract_planes(view_proj), is_camera);
		return Culling_mask(1) << (_frustums.size() - 1);
	}

} // namespace mirrage::renderer
