#include <mirrage/renderer/pass/blit_pass.hpp>


namespace mirrage::renderer {

	namespace {
		auto build_render_pass(Deferred_renderer&                 renderer,
		                       vk::DescriptorSetLayout            desc_set_layout,
		                       std::vector<graphic::Framebuffer>& out_framebuffers)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto screen =
			        builder.add_attachment(vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                                         renderer.swapchain().image_format(),
			                                                         vk::SampleCountFlagBits::e1,
			                                                         vk::AttachmentLoadOp::eDontCare,
			                                                         vk::AttachmentStoreOp::eStore,
			                                                         vk::AttachmentLoadOp::eDontCare,
			                                                         vk::AttachmentStoreOp::eDontCare,
			                                                         vk::ImageLayout::eUndefined,
			                                                         vk::ImageLayout::ePresentSrcKHR});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(renderer.noise_descriptor_set_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant(
			        "settings"_strid, sizeof(glm::vec4), vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(screen);

			pass.stage("blit"_strid)
			        .shader("frag_shader:blit"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:blit"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			for(auto& sc_image : renderer.swapchain().get_image_views()) {
				out_framebuffers.emplace_back(builder.build_framebuffer({*sc_image, util::Rgba{}},
				                                                        renderer.swapchain().image_width(),
				                                                        renderer.swapchain().image_height()));
			}

			return render_pass;
		}
	} // namespace


	Blit_pass::Blit_pass(Deferred_renderer& renderer, graphic::Render_target_2D& src)
	  : Render_pass(renderer)
	  , _src(src)
	  , _sampler(renderer.device().create_sampler(1,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eLinear,
	                                              vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(renderer.device(), *_sampler, 1)
	  , _descriptor_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(), {src.view(0)}))
	  , _render_pass(build_render_pass(renderer, *_descriptor_set_layout, _framebuffers))
	{
	}


	void Blit_pass::update(util::Time) {}

	void Blit_pass::post_draw(Frame_data& frame)
	{
		auto _ = _mark_subpass(frame);

		_render_pass.execute(frame.main_command_buffer, _framebuffers.at(frame.swapchain_image), [&] {
			auto descriptor_sets = std::array<vk::DescriptorSet, 3>{
			        frame.global_uniform_set, _renderer.noise_descriptor_set(), *_descriptor_set};

			_render_pass.bind_descriptor_sets(0, descriptor_sets);

			glm::vec4 settings;
			settings.z = _renderer.settings().exposure_override;

			_render_pass.push_constant("settings"_strid, settings);

			frame.main_command_buffer.draw(3, 1, 0, 0);
		});
	}


	auto Blit_pass_factory::create_pass(Deferred_renderer& renderer,
	                                    std::shared_ptr<void>,
	                                    util::maybe<ecs::Entity_manager&>,
	                                    Engine&,
	                                    bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		auto& color_src = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		return std::make_unique<Blit_pass>(renderer, color_src);
	}

	auto Blit_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Blit_pass_factory::configure_device(vk::PhysicalDevice,
	                                         util::maybe<std::uint32_t>,
	                                         graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
