#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>

#include <mirrage/renderer/animation_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/particle_system.hpp>
#include <mirrage/renderer/pass/deferred_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/renderer/model.hpp>

using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	namespace {
		auto create_input_attachment_descriptor_set_layout(graphic::Device& device)
		        -> vk::UniqueDescriptorSetLayout
		{
			auto binding = vk::DescriptorSetLayoutBinding{
			        0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment};

			return device.create_descriptor_set_layout(binding);
		}
	} // namespace

	Deferred_geometry_subpass::Deferred_geometry_subpass(Deferred_renderer& r, ecs::Entity_manager& entities)
	  : _ecs(entities)
	  , _renderer(r)
	  , _decal_input_attachment_descriptor_set_layout(
	            create_input_attachment_descriptor_set_layout(r.device()))
	  , _decal_input_attachment_descriptor_set(
	            r.create_descriptor_set(*_decal_input_attachment_descriptor_set_layout, 1))
	{
		entities.register_component_type<Billboard_comp>();
		entities.register_component_type<Decal_comp>();
		entities.register_component_type<Model_comp>();
		entities.register_component_type<Pose_comp>();
		entities.register_component_type<Shared_pose_comp>();

		auto depth_info = vk::DescriptorImageInfo(
		        vk::Sampler{}, r.gbuffer().depth.view(0), vk::ImageLayout::eShaderReadOnlyOptimal);

		auto desc_write = vk::WriteDescriptorSet{*_decal_input_attachment_descriptor_set,
		                                         0,
		                                         0,
		                                         1,
		                                         vk::DescriptorType::eInputAttachment,
		                                         &depth_info};

		r.device().vk_device()->updateDescriptorSets(1, &desc_write, 0, nullptr);
	}

	void Deferred_geometry_subpass::configure_pipeline(Deferred_renderer&             renderer,
	                                                   graphic::Pipeline_description& p)
	{
		p.rasterization.cullMode = vk::CullModeFlagBits::eNone; //
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.vertex<Model_vertex>(
		        0, false, 0, &Model_vertex::position, 1, &Model_vertex::normal, 2, &Model_vertex::tex_coords);
	}
	void Deferred_geometry_subpass::configure_subpass(Deferred_renderer&, graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);

		pass.stage("alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);
	}
	void Deferred_geometry_subpass::configure_emissive_subpass(Deferred_renderer&,
	                                                           graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);

		pass.stage("alphatest"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::configure_animation_pipeline(Deferred_renderer&             renderer,
	                                                             graphic::Pipeline_description& p)
	{

		p.rasterization.cullMode = vk::CullModeFlagBits::eNone;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.add_descriptor_set_layout(*renderer.gbuffer().animation_data_layout);
		p.vertex<Model_rigged_vertex>(0,
		                              false,
		                              0,
		                              &Model_rigged_vertex::position,
		                              1,
		                              &Model_rigged_vertex::normal,
		                              2,
		                              &Model_rigged_vertex::tex_coords,
		                              3,
		                              &Model_rigged_vertex::bone_ids,
		                              4,
		                              &Model_rigged_vertex::bone_weights);
	}
	void Deferred_geometry_subpass::configure_animation_subpass(Deferred_renderer&,
	                                                            graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);

		pass.stage("alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);


		pass.stage("dq_default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex);

		pass.stage("dq_alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex);
	}
	void Deferred_geometry_subpass::configure_animation_emissive_subpass(Deferred_renderer&,
	                                                                     graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);

		pass.stage("alphatest"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex);


		pass.stage("dq_default"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex);

		pass.stage("dq_alphatest"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::configure_billboard_pipeline(Deferred_renderer&             renderer,
	                                                             graphic::Pipeline_description& p)
	{
		p.rasterization.cullMode  = vk::CullModeFlagBits::eBack;
		p.input_assembly.topology = vk::PrimitiveTopology::eTriangleStrip;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
	}
	void Deferred_geometry_subpass::configure_billboard_subpass(Deferred_renderer&,
	                                                            graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:billboard_lit"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:billboard"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::configure_decal_pipeline(Deferred_renderer&             renderer,
	                                                         graphic::Pipeline_description& p)
	{
		p.rasterization.cullMode  = vk::CullModeFlagBits::eBack;
		p.input_assembly.topology = vk::PrimitiveTopology::eTriangleStrip;
		p.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{
                vk::PipelineDepthStencilStateCreateFlags{}, true, 0, vk::CompareOp::eGreaterOrEqual};
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.add_descriptor_set_layout(*_decal_input_attachment_descriptor_set_layout);
	}
	void Deferred_geometry_subpass::configure_decal_subpass(Deferred_renderer&,
	                                                        graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:decal"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:decal"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::configure_particle_pipeline(Deferred_renderer&             renderer,
	                                                            graphic::Pipeline_description& p)
	{
		p.rasterization.cullMode = vk::CullModeFlagBits::eNone;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.add_descriptor_set_layout(renderer.compute_storage_buffer_layout()); //< particle type data

		p.vertex<Model_vertex>(
		        0, false, 0, &Model_vertex::position, 1, &Model_vertex::normal, 2, &Model_vertex::tex_coords);
		p.vertex<Particle>(1, true, 3, &Particle::position, 4, &Particle::velocity, 5, &Particle::data);
	}
	void Deferred_geometry_subpass::configure_particle_subpass(Deferred_renderer&,
	                                                           graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:particle_solid"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:particle"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::on_render_pass_configured(graphic::Render_pass& pass,
	                                                          graphic::Framebuffer& framebuffer)
	{
		auto set = [&](auto& dst, int subpass, util::Str_id stage) {
			dst = {_renderer.reserve_secondary_command_buffer_group(),
			       pass.get_stage(subpass, stage, "dpc"_strid)};
		};

		set(_stages_model_static["default"_strid], 0, "default"_strid);
		set(_stages_model_static["alphatest"_strid], 0, "alphatest"_strid);

		set(_stages_model_anim["default"_strid], 1, "default"_strid);
		set(_stages_model_anim["alphatest"_strid], 1, "alphatest"_strid);
		set(_stages_model_anim["dq_default"_strid], 1, "dq_default"_strid);
		set(_stages_model_anim["dq_alphatest"_strid], 1, "dq_alphatest"_strid);

		set(_stages_model_static_emissive["default"_strid], 2, "default"_strid);
		set(_stages_model_static_emissive["alphatest"_strid], 2, "alphatest"_strid);

		set(_stages_model_anim_emissive["default"_strid], 3, "default"_strid);
		set(_stages_model_anim_emissive["alphatest"_strid], 3, "alphatest"_strid);
		set(_stages_model_anim_emissive["dq_default"_strid], 3, "dq_default"_strid);
		set(_stages_model_anim_emissive["dq_alphatest"_strid], 3, "dq_alphatest"_strid);

		set(_stage_decals, 4, "default"_strid);
		set(_stage_billboards, 5, "default"_strid);
		set(_stage_particles, 6, "default"_strid);

		_render_pass = pass;
		_framebuffer = framebuffer;
	}


	void Deferred_geometry_subpass::update(util::Time) {}

	void Deferred_geometry_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto commands = frame.main_command_buffer;

		auto execute = [&](auto& group_map) {
			for(auto&& [_, g] : group_map)
				_renderer.execute_group(g.group, commands);
		};

		execute(_stages_model_static);

		render_pass.next_subpass(vk::SubpassContents::eSecondaryCommandBuffers);
		execute(_stages_model_anim);

		render_pass.next_subpass(vk::SubpassContents::eSecondaryCommandBuffers);
		execute(_stages_model_static_emissive);

		render_pass.next_subpass(vk::SubpassContents::eSecondaryCommandBuffers);
		execute(_stages_model_anim_emissive);

		render_pass.next_subpass(vk::SubpassContents::eSecondaryCommandBuffers);
		_renderer.execute_group(_stage_decals.group, commands);

		render_pass.next_subpass(vk::SubpassContents::eSecondaryCommandBuffers);
		_renderer.execute_group(_stage_billboards.group, commands);

		render_pass.next_subpass(vk::SubpassContents::eSecondaryCommandBuffers);
		_renderer.execute_group(_stage_particles.group, commands);
	}


	namespace {
		auto create_model_pcs(Deferred_renderer& renderer,
		                      ecs::Entity_facet  entity,
		                      const glm::vec4&   emissive_color,
		                      const glm::mat4&   transform)
		{
			Deferred_push_constants dpc{};
			dpc.model      = renderer.global_uniforms().view_mat * transform;
			dpc.light_data = emissive_color;
			dpc.light_data.a /= 10000.0f;

			return dpc;
		}
	} // namespace

	void Deferred_geometry_subpass::handle_obj(Frame_data&       frame,
	                                           Culling_mask      mask,
	                                           ecs::Entity_facet entity,
	                                           const glm::vec4&  emissive_color,
	                                           const glm::mat4&  transform,
	                                           const Model&      model,
	                                           const Sub_mesh&   sub_mesh)
	{
		if((mask & frame.camera_culling_mask) == 0)
			return;

		auto& material = *sub_mesh.material;
		auto  pcs      = create_model_pcs(_renderer, entity, emissive_color, transform);

		auto draw = [&](auto& stage) {
			auto [cmd_buffer, _] = _get_cmd_buffer(frame, stage);

			stage.stage.bind_descriptor_set(cmd_buffer, 1, material.desc_set());

			model.bind_mesh(cmd_buffer, 0);

			stage.stage.push_constant(cmd_buffer, pcs);

			cmd_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		};

		draw(_stages_model_static[material.substance_id()]);
		if(material.has_emission()) {
			draw(_stages_model_static_emissive[material.substance_id()]);
		}
	}

	void Deferred_geometry_subpass::handle_obj(Frame_data&       frame,
	                                           Culling_mask      mask,
	                                           ecs::Entity_facet entity,
	                                           const glm::vec4&  emissive_color,
	                                           const glm::mat4&  transform,
	                                           const Model&      model,
	                                           Skinning_type     skinning_type,
	                                           std::uint32_t     pose_offset)
	{
		if((mask & frame.camera_culling_mask) == 0)
			return;

		auto pcs = create_model_pcs(_renderer, entity, emissive_color, transform);

		for(auto& sub_mesh : model.sub_meshes()) {
			auto& material = *sub_mesh.material;

			auto draw = [&](auto& stage_map) {
				auto substance = material.substance_id();
				if(skinning_type == Skinning_type::dual_quaternion_skinning)
					substance = "dq_"_strid + substance;

				auto& stage = stage_map[substance];

				auto [cmd_buffer, _] = _get_cmd_buffer(frame, stage);

				stage.stage.bind_descriptor_set(cmd_buffer, 1, material.desc_set());

				stage.stage.bind_descriptor_set(
				        cmd_buffer, 2, _renderer.gbuffer().animation_data, {&pose_offset, 1u});

				model.bind_mesh(cmd_buffer, 0);

				stage.stage.push_constant(cmd_buffer, pcs);

				cmd_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
			};

			draw(_stages_model_anim);
			if(material.has_emission()) {
				draw(_stages_model_anim_emissive);
			}
		}
	}

	void Deferred_geometry_subpass::handle_obj(Frame_data&      frame,
	                                           Culling_mask     mask,
	                                           const Billboard& bb,
	                                           const glm::vec3& pos)
	{
		if((mask & frame.camera_culling_mask) == 0 || !bb.dynamic_lighting)
			return;

		auto [cmd_buffer, _] = _get_cmd_buffer(frame, _stage_billboards);

		_stage_billboards.stage.bind_descriptor_set(cmd_buffer, 1, bb.material->desc_set());

		auto pcs = construct_push_constants(
		        bb, pos, _renderer.global_uniforms().view_mat, _renderer.window().viewport());

		_stage_billboards.stage.push_constant(cmd_buffer, pcs);
		cmd_buffer.draw(4, 1, 0, 0);
	}

	void Deferred_geometry_subpass::handle_obj(Frame_data&      frame,
	                                           Culling_mask     mask,
	                                           const Decal&     decal,
	                                           const glm::mat4& transform)
	{
		if((mask & frame.camera_culling_mask) == 0)
			return;

		auto [cmd_buffer, fresh] = _get_cmd_buffer(frame, _stage_decals);

		_stage_decals.stage.bind_descriptor_set(cmd_buffer, 1, decal.material->desc_set());

		if(fresh) {
			_stage_decals.stage.bind_descriptor_set(cmd_buffer, 2, *_decal_input_attachment_descriptor_set);
		}

		auto blend = std::array<float, 4>{0, 0, 0, 0};
		if(decal.material->has_normal())
			blend[0] = blend[1] = decal.normal_alpha;
		if(decal.material->has_brdf()) {
			blend[2] = decal.roughness_alpha;
			blend[3] = decal.metallic_alpha;
		}
		cmd_buffer.setBlendConstants(blend.data());

		auto pcs = construct_push_constants(decal, _renderer.global_uniforms().view_mat * transform);

		_stage_decals.stage.push_constant(cmd_buffer, pcs);
		cmd_buffer.draw(14, 1, 0, 0);
	}

	void Deferred_geometry_subpass::handle_obj(Frame_data&  frame,
	                                           Culling_mask mask,
	                                           const glm::vec4&,
	                                           const Particle_system&  particle_sys_comp,
	                                           const Particle_emitter& particle_emitter)
	{
		auto& type_cfg = *particle_emitter.cfg().type;

		if((mask & frame.camera_culling_mask) == 0 || !_renderer.billboard_model().ready()
		   || type_cfg.blend != Particle_blend_mode::solid || !particle_emitter.drawable()
		   || particle_emitter.particle_count() <= 0)
			return;

		auto [cmd_buffer, _] = _get_cmd_buffer(frame, _stage_particles);

		auto desc_sets = std::array<vk::DescriptorSet, 2>{type_cfg.material->desc_set(),
		                                                  particle_emitter.particle_uniforms()};
		_stage_particles.stage.bind_descriptor_sets(cmd_buffer, 1, desc_sets);

		// bind model
		auto model = type_cfg.model ? &*type_cfg.model : &_renderer.billboard_model();
		model->bind_mesh(cmd_buffer, 0);

		// bind particle data
		cmd_buffer.bindVertexBuffers(
		        1,
		        {particle_emitter.particle_buffer()},
		        {vk::DeviceSize(particle_emitter.particle_offset()) * vk::DeviceSize(sizeof(Particle))});

		Deferred_push_constants dpc{};
		dpc.model = glm::mat4(1);
		if(type_cfg.geometry == Particle_geometry::billboard) {
			dpc.model    = glm::inverse(_renderer.global_uniforms().view_mat);
			dpc.model[3] = glm::vec4(0, 0, 0, 1);
		}
		_stage_particles.stage.push_constant(cmd_buffer, dpc);

		// draw instanced
		auto& sub_mesh = model->sub_meshes().at(0);
		cmd_buffer.drawIndexed(sub_mesh.index_count,
		                       std::uint32_t(particle_emitter.particle_count()),
		                       sub_mesh.index_offset,
		                       0,
		                       0);
	}

	auto Deferred_geometry_subpass::_get_cmd_buffer(Frame_data& frame, Stage_data& stage_ref)
	        -> std::pair<vk::CommandBuffer, bool>
	{
		auto [cmd_buffer, empty] = _renderer.get_secondary_command_buffer(stage_ref.group);
		if(empty) {
			stage_ref.stage.begin(cmd_buffer, _framebuffer.get_or_throw());
			stage_ref.stage.bind_descriptor_set(cmd_buffer, 0, frame.global_uniform_set);
		}

		return {cmd_buffer, empty};
	}

} // namespace mirrage::renderer
