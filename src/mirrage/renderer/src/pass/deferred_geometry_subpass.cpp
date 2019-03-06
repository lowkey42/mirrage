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

		auto create_billboard_model(Deferred_renderer& r)
		{
			const auto vertices = std::array<Model_vertex, 4>{
			        Model_vertex{glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec2(0, 1)},
			        Model_vertex{glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec2(1, 1)},
			        Model_vertex{glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec2(0, 0)},
			        Model_vertex{glm::vec3(1, 1, 0), glm::vec3(0, 0, 1), glm::vec2(1, 0)}};
			const auto indices = std::array<std::uint32_t, 6>{0, 1, 2, 2, 1, 3};

			return Model{graphic::Mesh{r.device(), r.queue_family(), vertices, indices},
			             {Sub_mesh{{}, 0u, 6u, glm::vec3(0, 0, 0), 1.f}},
			             1.f,
			             glm::vec3(0, 0, 0),
			             false,
			             0};
		}

	} // namespace

	Deferred_geometry_subpass::Deferred_geometry_subpass(Deferred_renderer& r, ecs::Entity_manager& entities)
	  : _ecs(entities)
	  , _renderer(r)
	  , _particle_billboard(create_billboard_model(r))
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
		p.rasterization.cullMode  = vk::CullModeFlagBits::eNone;
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
		p.add_descriptor_set_layout(renderer.compute_uniform_buffer_layout()); //< particle type data

		p.vertex<Model_vertex>(
		        0, false, 0, &Model_vertex::position, 1, &Model_vertex::normal, 2, &Model_vertex::tex_coords);
		p.vertex<Particle>(1, true, 3, &Particle::position, 4, &Particle::velocity, 5, &Particle::ttl);
	}
	void Deferred_geometry_subpass::configure_particle_subpass(Deferred_renderer&,
	                                                           graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:particle_solid"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:particle"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_geometry_subpass::update(util::Time) {}

	void Deferred_geometry_subpass::pre_draw(Frame_data& frame)
	{
		// select relevant draw commands and partition into normal and rigged geometry
		auto geo_range = frame.partition_geometry(1u);

		auto rigged_begin = std::find_if(
		        geo_range.begin(), geo_range.end(), [](auto& geo) { return geo.model->rigged(); });

		_geometry_range        = util::range(geo_range.begin(), rigged_begin);
		_rigged_geometry_range = util::range(rigged_begin, geo_range.end());
	}

	void Deferred_geometry_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Geometry");

		Deferred_push_constants dpc{};

		auto last_substance_id = ""_strid;
		auto last_material     = static_cast<const Material*>(nullptr);
		auto last_model        = static_cast<const Model*>(nullptr);

		auto prepare_draw = [&](auto& geo, bool emissive) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);

			if(geo.substance_id != last_substance_id) {
				last_substance_id = geo.substance_id;
				render_pass.set_stage(geo.substance_id);
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
			dpc.model    = _renderer.global_uniforms().view_mat * dpc.model;

			if(emissive) {
				auto emissive_color = glm::vec4(1, 1, 1, 1000);

				if(auto entity = _ecs.get(geo.entity); entity.is_some()) {
					emissive_color = entity.get_or_throw().template get<Material_property_comp>().process(
					        emissive_color, [](auto& m) { return m.emissive_color; });
				}

				emissive_color.a /= 10000.0f;
				dpc.light_data = emissive_color;
			}
		};

		auto next_sub_pass = [&] {
			render_pass.next_subpass();
			last_substance_id = ""_strid;
			last_material     = static_cast<const Material*>(nullptr);
			last_model        = static_cast<const Model*>(nullptr);
		};

		for(bool emissive_pass : {false, true}) {
			// draw all static models
			for(auto& geo : _geometry_range) {
				auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
				if(emissive_pass && !sub_mesh.material->has_emission())
					continue;

				prepare_draw(geo, emissive_pass);
				render_pass.push_constant("dpc"_strid, dpc);
				frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
			}

			next_sub_pass();

			// draw all animated models in a new subpass
			for(auto& geo : _rigged_geometry_range) {
				if(geo.animation_uniform_offset.is_nothing())
					continue;

				auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
				if(emissive_pass && !sub_mesh.material->has_emission())
					continue;

				prepare_draw(geo, emissive_pass);

				render_pass.push_constant("dpc"_strid, dpc);

				auto uniform_offset = geo.animation_uniform_offset.get_or_throw();
				render_pass.bind_descriptor_set(2, _renderer.gbuffer().animation_data, {&uniform_offset, 1u});

				frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
			}

			next_sub_pass();
		}

		// draw decals
		std::sort(frame.decal_queue.begin(), frame.decal_queue.end(), [](auto& lhs, auto& rhs) {
			return &*std::get<0>(lhs).material < &*std::get<0>(rhs).material;
		});
		render_pass.set_stage("default"_strid);
		render_pass.bind_descriptor_set(2, *_decal_input_attachment_descriptor_set);
		for(auto&& [decal, model_mat] : frame.decal_queue) {
			if(&*decal.material != last_material) {
				last_material = &*decal.material;
				last_material->bind(render_pass);

				auto blend = std::array<float, 4>{0, 0, 0, 0};
				if(decal.material->has_normal())
					blend[0] = blend[1] = decal.normal_alpha;
				if(decal.material->has_brdf()) {
					blend[2] = decal.roughness_alpha;
					blend[3] = decal.metallic_alpha;
				}
				frame.main_command_buffer.setBlendConstants(blend.data());
			}

			auto pcs = construct_push_constants(decal, _renderer.global_uniforms().view_mat * model_mat);

			render_pass.push_constant("dpc"_strid, pcs);
			frame.main_command_buffer.draw(14, 1, 0, 0);
		}
		next_sub_pass();

		// draw billboards
		std::sort(frame.billboard_queue.begin(), frame.billboard_queue.end(), [](auto& lhs, auto& rhs) {
			return std::make_pair(lhs.dynamic_lighting ? 0 : 1, &*lhs.material)
			       < std::make_pair(rhs.dynamic_lighting ? 0 : 1, &*rhs.material);
		});
		render_pass.set_stage("default"_strid);
		for(auto&& bb : frame.billboard_queue) {
			if(!bb.dynamic_lighting)
				break;

			if(&*bb.material != last_material) {
				last_material = &*bb.material;
				last_material->bind(render_pass);
			}

			auto pcs = construct_push_constants(
			        bb, _renderer.global_uniforms().view_mat, _renderer.window().viewport());

			render_pass.push_constant("dpc"_strid, pcs);
			frame.main_command_buffer.draw(4, 1, 0, 0);
		}
		next_sub_pass();

		render_pass.set_stage("default"_strid);
		if(_particle_billboard.ready()) {
			for(auto&& particle : frame.particle_queue) {
				if(particle.type_cfg->blend != Particle_blend_mode::solid || !particle.emitter->drawable())
					break;

				auto material = &*particle.type_cfg->material;
				if(material != last_material) {
					last_material = material;
					last_material->bind(render_pass);
				}

				// bind emitter data
				render_pass.bind_descriptor_set(2, particle.emitter->particle_uniforms());

				// bind model
				auto model = particle.type_cfg->model ? &*particle.type_cfg->model : &_particle_billboard;
				if(model != last_model) {
					last_model = model;
					last_model->bind_mesh(frame.main_command_buffer, 0);
				}

				// bind particle data
				frame.main_command_buffer.bindVertexBuffers(
				        1,
				        {particle.emitter->particle_buffer()},
				        {std::uint32_t(particle.emitter->particle_offset())});

				dpc.model = _renderer.global_uniforms().view_mat;
				render_pass.push_constant("dpc"_strid, dpc);

				// draw instanced
				auto& sub_mesh = last_model->sub_meshes().at(0);
				frame.main_command_buffer.drawIndexed(sub_mesh.index_count,
				                                      std::uint32_t(particle.emitter->particle_count()),
				                                      sub_mesh.index_offset,
				                                      0,
				                                      0);
			}
		}
	}
} // namespace mirrage::renderer
