#include <mirrage/renderer/render_pass.hpp>

#include <mirrage/renderer/model_comp.hpp>


namespace mirrage::renderer {
	auto Frame_data::partition_geometry(std::uint32_t mask) -> util::vector_range<Geometry>
	{

		// partition draw commands into normal and rigged geometry
		auto end       = std::partition(geometry_queue.begin(), geometry_queue.end(), [=](auto& geo) {
            return (geo.culling_mask & mask) != 0;
        });
		auto geo_range = util::range(geometry_queue.begin(), end);

		// sort geometry for efficient rendering
		std::sort(geo_range.begin(), geo_range.end(), [](auto& lhs, auto& rhs) {
			auto lhs_mat = &*lhs.model->sub_meshes()[lhs.sub_mesh].material;
			auto rhs_mat = &*rhs.model->sub_meshes()[rhs.sub_mesh].material;

			return std::make_tuple(lhs.model->rigged(), lhs.substance_id, lhs_mat, lhs.model)
			       < std::make_tuple(rhs.model->rigged(), rhs.substance_id, rhs_mat, rhs.model);
		});

		return geo_range;
	}

} // namespace mirrage::renderer
