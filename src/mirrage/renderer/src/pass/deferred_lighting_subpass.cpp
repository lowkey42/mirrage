#include <mirrage/renderer/pass/deferred_lighting_subpass.hpp>

#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/pass/deferred_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	using namespace util::unit_literals;
	using ecs::components::Transform_comp;

	namespace {
		constexpr auto num_input_attachments = 3;

		struct Vertex {
			glm::vec3 p;
		};

		auto create_input_attachment_descriptor_set_layout(graphic::Device& device)
		        -> vk::UniqueDescriptorSetLayout
		{
			auto bindings = std::array<vk::DescriptorSetLayoutBinding, num_input_attachments>();
			std::fill_n(
			        bindings.begin(),
			        num_input_attachments,
			        vk::DescriptorSetLayoutBinding{
			                0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment});

			for(auto i = std::uint32_t(0); i < num_input_attachments; i++) {
				bindings[i].binding = i;
			}

			return device.create_descriptor_set_layout(bindings);
		}

		auto create_point_light_mesh(graphic::Device& device, std::uint32_t owner_qfamily)
		{
			const auto t = (1.0f + std::sqrt(5.0f)) / 2.0f;

			auto vertices = std::vector<glm::vec3>{glm::normalize(glm::vec3{-1, t, 0}),
			                                       glm::normalize(glm::vec3{1, t, 0}),
			                                       glm::normalize(glm::vec3{-1, -t, 0}),
			                                       glm::normalize(glm::vec3{1, -t, 0}),
			                                       glm::normalize(glm::vec3{0, -1, t}),
			                                       glm::normalize(glm::vec3{0, 1, t}),
			                                       glm::normalize(glm::vec3{0, -1, -t}),
			                                       glm::normalize(glm::vec3{0, 1, -t}),
			                                       glm::normalize(glm::vec3{t, 0, -1}),
			                                       glm::normalize(glm::vec3{t, 0, 1}),
			                                       glm::normalize(glm::vec3{-t, 0, -1}),
			                                       glm::normalize(glm::vec3{-t, 0, 1})};
			auto indices  = std::vector<std::uint32_t>();

			auto add_triange = [&](int a, int b, int c) {
				indices.emplace_back(a);
				indices.emplace_back(b);
				indices.emplace_back(c);
			};

			// 5 faces around point 0
			add_triange(0, 11, 5);
			add_triange(0, 5, 1);
			add_triange(0, 1, 7);
			add_triange(0, 7, 10);
			add_triange(0, 10, 11);

			// 5 adjacent faces
			add_triange(1, 5, 9);
			add_triange(5, 11, 4);
			add_triange(11, 10, 2);
			add_triange(10, 7, 6);
			add_triange(7, 1, 8);

			// 5 faces around point 3
			add_triange(3, 9, 4);
			add_triange(3, 4, 2);
			add_triange(3, 2, 6);
			add_triange(3, 6, 8);
			add_triange(3, 8, 9);

			// 5 adjacent faces
			add_triange(4, 9, 5);
			add_triange(2, 4, 11);
			add_triange(6, 2, 10);
			add_triange(8, 6, 7);
			add_triange(9, 8, 1);

			return graphic::Mesh(device, owner_qfamily, vertices, indices);
		}

	} // namespace

	Deferred_lighting_subpass::Deferred_lighting_subpass(Deferred_renderer&   renderer,
	                                                     ecs::Entity_manager& entities,
	                                                     graphic::Texture_2D&)
	  : _ecs(entities)
	  , _renderer(renderer)
	  , _gbuffer(renderer.gbuffer())
	  , _point_light_mesh(create_point_light_mesh(renderer.device(), renderer.queue_family()))
	  , _input_attachment_descriptor_set_layout(
	            create_input_attachment_descriptor_set_layout(renderer.device()))
	  , _input_attachment_descriptor_set(renderer.create_descriptor_set(
	            *_input_attachment_descriptor_set_layout, num_input_attachments))
	{
		entities.register_component_type<Directional_light_comp>();
		entities.register_component_type<Point_light_comp>();

		auto& gbuffer    = renderer.gbuffer();
		auto  depth_info = vk::DescriptorImageInfo(
                vk::Sampler{}, gbuffer.depth.view(0), vk::ImageLayout::eShaderReadOnlyOptimal);
		auto albedo_mat_id_info = vk::DescriptorImageInfo(
		        vk::Sampler{}, gbuffer.albedo_mat_id.view(0), vk::ImageLayout::eShaderReadOnlyOptimal);
		auto mat_data_info = vk::DescriptorImageInfo(
		        vk::Sampler{}, gbuffer.mat_data.view(0), vk::ImageLayout::eShaderReadOnlyOptimal);

		auto desc_writes = std::array<vk::WriteDescriptorSet, num_input_attachments>();
		desc_writes[0]   = vk::WriteDescriptorSet{
                *_input_attachment_descriptor_set, 0, 0, 1, vk::DescriptorType::eInputAttachment, &depth_info};
		desc_writes[1] = vk::WriteDescriptorSet{*_input_attachment_descriptor_set,
		                                        1,
		                                        0,
		                                        1,
		                                        vk::DescriptorType::eInputAttachment,
		                                        &albedo_mat_id_info};
		desc_writes[2] = vk::WriteDescriptorSet{*_input_attachment_descriptor_set,
		                                        2,
		                                        0,
		                                        1,
		                                        vk::DescriptorType::eInputAttachment,
		                                        &mat_data_info};

		renderer.device().vk_device()->updateDescriptorSets(
		        desc_writes.size(), desc_writes.data(), 0, nullptr);
	}

	void Deferred_lighting_subpass::configure_pipeline(Deferred_renderer&             renderer,
	                                                   graphic::Pipeline_description& p)
	{
		p.add_descriptor_set_layout(*_input_attachment_descriptor_set_layout);
		p.add_descriptor_set_layout(*renderer.gbuffer().shadowmaps_layout);
	}

	void Deferred_lighting_subpass::configure_subpass(Deferred_renderer&        renderer,
	                                                  graphic::Subpass_builder& pass)
	{
		pass.stage("light_dir"_strid)
		        .shader("frag_shader:light_directional"_aid,
		                graphic::Shader_stage::fragment,
		                "main",
		                0,
		                renderer.settings().shadow_quality,
		                1,
		                renderer.gbuffer().shadowmapping_enabled ? 1 : 0)
		        .shader("vert_shader:light_directional"_aid, graphic::Shader_stage::vertex);

		auto& point_light_pipeline =
		        pass.stage("light_point"_strid)
		                .shader("frag_shader:light_point"_aid, graphic::Shader_stage::fragment)
		                .shader("vert_shader:light_point"_aid, graphic::Shader_stage::vertex)
		                .pipeline();

		point_light_pipeline.vertex<glm::vec3>(0, false);
		point_light_pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
		        vk::PipelineDepthStencilStateCreateFlags{}, true, 0, vk::CompareOp::eGreaterOrEqual};
		point_light_pipeline.rasterization.setCullMode(vk::CullModeFlagBits::eFront);
	}

	void Deferred_lighting_subpass::update(util::Time) {}

	void Deferred_lighting_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Lighting");

		render_pass.set_stage("light_dir"_strid);

		render_pass.bind_descriptor_sets(1, {_input_attachment_descriptor_set.get_ptr(), 1});
		render_pass.bind_descriptor_set(2, *_gbuffer.shadowmaps);

		Deferred_push_constants dpc{};

		auto inv_view = _renderer.global_uniforms().inv_view_mat;

		// directional light
		for(auto& light : frame.light_queue) {
			if(auto ll = std::get_if<Directional_light_comp*>(&light.light); ll) {
				auto& light_data = **ll;

				dpc.model        = light_data.calc_shadowmap_view_proj(*light.transform) * inv_view;
				dpc.light_color  = glm::vec4(light_data.color(), light_data.intensity() / 10000.0f);
				dpc.light_data.r = light_data.source_radius() / 1_m;
				auto dir =
				        _renderer.global_uniforms().view_mat * glm::vec4(-light.transform->direction(), 0.f);
				auto dir_len      = glm::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
				dpc.light_data.g  = dir.x / dir_len;
				dpc.light_data.b  = dir.y / dir_len;
				dpc.light_data.a  = dir.z / dir_len;
				dpc.light_data2.r = _gbuffer.shadowmaps ? light_data.shadowmap_id() : -1;

				render_pass.push_constant("dpc"_strid, dpc);

				frame.main_command_buffer.draw(3, 1, 0, 0);
			}
		}

		static auto first = true;
		if(first) {
			first = false;
			return;
		}

		_renderer.device().wait_idle();

		// point light
		if(_point_light_mesh.ready()) {
			auto first_point_light = true;

			for(auto& light : frame.light_queue) {
				if(auto ll = std::get_if<Point_light_comp*>(&light.light); ll) {
					auto& light_data = **ll;

					if(first_point_light) {
						first_point_light = false;
						render_pass.set_stage("light_point"_strid);
						_point_light_mesh.bind(frame.main_command_buffer, 0);
					}

					dpc.model = _renderer.global_uniforms().view_proj_mat
					            * glm::translate(glm::mat4(1), light.transform->position)
					            * glm::scale(glm::mat4(1.f), glm::vec3(light_data.calc_radius()));
					dpc.light_color  = glm::vec4(light_data.color(), light_data.intensity() / 10000.0f);
					dpc.light_data.r = light_data.source_radius() / 1_m;
					auto pos =
					        _renderer.global_uniforms().view_mat * glm::vec4(light.transform->position, 1.f);
					pos /= pos.w;
					dpc.light_data.g  = pos.x;
					dpc.light_data.b  = pos.y;
					dpc.light_data.a  = pos.z;
					dpc.light_data2.g = _gbuffer.colorA.width();
					dpc.light_data2.b = _gbuffer.colorA.height();
					render_pass.push_constant("dpc"_strid, dpc);

					frame.main_command_buffer.drawIndexed(_point_light_mesh.index_count(), 1, 0, 0, 0);
				}
			}
		}
	}
} // namespace mirrage::renderer
