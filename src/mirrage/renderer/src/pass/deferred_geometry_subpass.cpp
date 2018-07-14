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

		auto last_mat_id   = ""_strid;
		auto last_material = static_cast<const Material*>(nullptr);
		auto last_model    = static_cast<const Model*>(nullptr);

		for(auto& geo : geo_range) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
			auto  mat_id   = sub_mesh.material->material_id();

			if(sub_mesh.material->material_id() != last_mat_id) {
				last_mat_id = sub_mesh.material->material_id();

				if(_renderer.settings().debug_disect && (!mat_id || mat_id == "default"_strid)) {
					render_pass.set_stage("alpha_test"_strid);

				} else if(!mat_id) {
					render_pass.set_stage("default"_strid);
				} else {
					render_pass.set_stage(mat_id);
				}
			}

			if(&*sub_mesh.material != last_material) {
				last_material = &*sub_mesh.material;
				last_material->bind(render_pass);
			}

			if(geo.model != last_model) {
				last_model = geo.model;
				geo.model->bind_mesh(frame.main_command_buffer, 0);
			}

			dpc.model    = glm::toMat4(geo.orientation) * glm::scale(glm::mat4(1.f), geo.scale);
			dpc.model[3] = glm::vec4(geo.position, 1.f);
			render_pass.push_constant("dpc"_strid, dpc);

			frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}
	}
} // namespace mirrage::renderer
