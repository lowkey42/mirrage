#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>

#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/pass/deferred_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/renderer/model.hpp>

using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	Deferred_geometry_subpass::Deferred_geometry_subpass(Deferred_renderer& r, ecs::Entity_manager& entities)
	  : _ecs(entities), _renderer(r)
	{
		entities.register_component_type<Model_comp>();
	}

	void Deferred_geometry_subpass::configure_pipeline(Deferred_renderer&             renderer,
	                                                   graphic::Pipeline_description& p)
	{

		p.rasterization.cullMode = vk::CullModeFlagBits::eNone;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.vertex<Model_vertex>(
		        0, false, 0, &Model_vertex::position, 1, &Model_vertex::normal, 2, &Model_vertex::tex_coords);
	}
	void Deferred_geometry_subpass::configure_subpass(Deferred_renderer&, graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);

		pass.stage("emissive"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);

		pass.stage("alpha_test"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::update(util::Time) {}
	void Deferred_geometry_subpass::pre_draw(Frame_data&) {}

	void Deferred_geometry_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Geometry");


		auto end = std::partition(frame.geometry_queue.begin(), frame.geometry_queue.end(), [](auto& geo) {
			return (geo.culling_mask & 1u) != 0;
		});
		auto geo_range = util::range(frame.geometry_queue.begin(), end);

		std::sort(geo_range.begin(), geo_range.end(), [&](auto& lhs, auto& rhs) {
			auto lhs_mat = &*lhs.model->sub_meshes()[lhs.sub_mesh].material;
			auto rhs_mat = &*rhs.model->sub_meshes()[rhs.sub_mesh].material;

			return std::make_tuple(lhs_mat->material_id(), lhs_mat, lhs.model)
			       < std::make_tuple(rhs_mat->material_id(), rhs_mat, rhs.model);
		});

		// TODO: sort by depth, too
		/*
		auto eye = _renderer.active_camera().get_or_throw().eye_position;
		std::sort(geo_range.begin(), geo_range.end(), [&](auto& lhs, auto& rhs) {
			return glm::distance2(eye, lhs.position) < glm::distance2(eye, rhs.position);
		});
		*/


		Deferred_push_constants dpc{};
		dpc.light_data.x = _renderer.settings().debug_disect;

		for(auto& geo : geo_range) {
			dpc.model    = glm::toMat4(geo.orientation) * glm::scale(glm::mat4(1.f), geo.scale);
			dpc.model[3] = glm::vec4(geo.position, 1.f);

			geo.model->bind_mesh(frame.main_command_buffer, 0);
			auto [offset, count, material] = geo.model->bind_sub_mesh(render_pass, geo.sub_mesh);

			if(_renderer.settings().debug_disect
			   && (!material->material_id() || material->material_id() == "default"_strid)) {
				render_pass.set_stage("alpha_test"_strid);

			} else if(!material->material_id()) {
				render_pass.set_stage("default"_strid);
			} else {
				render_pass.set_stage(material->material_id());
			}

			render_pass.push_constant("dpc"_strid, dpc);
			frame.main_command_buffer.drawIndexed(count, 1, offset, 0, 0);
		}
	}
} // namespace mirrage::renderer
