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
	}

	void Deferred_geometry_subpass::update(util::Time) {}
	void Deferred_geometry_subpass::pre_draw(vk::CommandBuffer&) {}

	void Deferred_geometry_subpass::draw(vk::CommandBuffer& command_buffer, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Geometry");

		Deferred_push_constants dpc{};

		for(auto& [model, transform] : _ecs.list<Model_comp, Transform_comp>()) {
			dpc.model        = transform.to_mat4();
			dpc.light_data.x = _renderer.settings().debug_disect;
			render_pass.push_constant("dpc"_strid, dpc);

			model.model()->bind(command_buffer, render_pass, 0, [&](auto& material, auto offset, auto count) {
				if(!material->material_id()) {
					render_pass.set_stage("default"_strid);
				} else {
					render_pass.set_stage(material->material_id());
				}

				command_buffer.drawIndexed(count, 1, offset, 0, 0);
			});
		}
	}
} // namespace mirrage::renderer
