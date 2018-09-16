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
	} // namespace

	Deferred_lighting_subpass::Deferred_lighting_subpass(Deferred_renderer&   renderer,
	                                                     ecs::Entity_manager& entities,
	                                                     graphic::Texture_2D&)
	  : _ecs(entities)
	  , _renderer(renderer)
	  , _gbuffer(renderer.gbuffer())
	  , _input_attachment_descriptor_set_layout(
	            create_input_attachment_descriptor_set_layout(renderer.device()))
	  , _input_attachment_descriptor_set(renderer.create_descriptor_set(
	            *_input_attachment_descriptor_set_layout, num_input_attachments))
	{
		entities.register_component_type<Directional_light_comp>();

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
		if(renderer.gbuffer().shadowmaps_layout) {
			p.add_descriptor_set_layout(*renderer.gbuffer().shadowmaps_layout);
		}
	}

	void Deferred_lighting_subpass::configure_subpass(Deferred_renderer&        renderer,
	                                                  graphic::Subpass_builder& pass)
	{
		pass.stage("light_dir"_strid)
		        .shader("frag_shader:light_directional"_aid,
		                graphic::Shader_stage::fragment,
		                "main",
		                0,
		                renderer.settings().shadow_quality)
		        .shader("vert_shader:light_directional"_aid, graphic::Shader_stage::vertex);
	}

	void Deferred_lighting_subpass::update(util::Time) {}

	void Deferred_lighting_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Lighting");

		render_pass.set_stage("light_dir"_strid);

		render_pass.bind_descriptor_sets(1, {_input_attachment_descriptor_set.get_ptr(), 1});

		if(_gbuffer.shadowmaps) {
			auto desc_set = *_gbuffer.shadowmaps;
			render_pass.bind_descriptor_sets(2, {&desc_set, 1});
		}

		Deferred_push_constants dpc{};

		auto inv_view = _renderer.global_uniforms().inv_view_mat;

		for(auto& light : frame.light_queue) {
			if(auto ll = std::get_if<Directional_light_comp*>(&light.light); ll) {
				auto& light_data = **ll;

				dpc.model        = light_data.calc_shadowmap_view_proj(*light.transform) * inv_view;
				dpc.light_color  = glm::vec4(light_data.color(), light_data.intensity());
				dpc.light_data.r = light_data.source_radius() / 1_m;
				auto dir =
				        _renderer.global_uniforms().view_mat * glm::vec4(-light.transform->direction(), 0.f);
				dpc.light_data.g  = dir.x;
				dpc.light_data.b  = dir.y;
				dpc.light_data.a  = dir.z;
				dpc.light_data2.r = _gbuffer.shadowmaps ? light_data.shadowmap_id() : -1;

				render_pass.push_constant("dpc"_strid, dpc);

				frame.main_command_buffer.draw(3, 1, 0, 0);
			}
		}
	}
} // namespace mirrage::renderer
