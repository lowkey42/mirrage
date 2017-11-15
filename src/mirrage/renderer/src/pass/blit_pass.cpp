#include <mirrage/renderer/pass/blit_pass.hpp>


namespace mirrage::renderer {

	namespace {
		auto build_render_pass(Deferred_renderer&                 renderer,
		                       vk::DescriptorSetLayout            desc_set_layout,
		                       std::vector<graphic::Framebuffer>& out_framebuffers) {

			auto builder = renderer.device().create_render_pass_builder();

			auto screen = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
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
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant(
			        "settings"_strid, sizeof(glm::vec4), vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(screen);

			pass.stage("blit"_strid)
			        .shader("frag_shader:blit"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:blit"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite);

			builder.add_dependency(pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing,
			                       vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eShaderRead
			                               | vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			for(auto& sc_image : renderer.swapchain().get_images()) {
				out_framebuffers.emplace_back(
				        builder.build_framebuffer({*sc_image, util::Rgba{}},
				                                  renderer.swapchain().image_width(),
				                                  renderer.swapchain().image_height()));
			}

			return render_pass;
		}
	} // namespace


	Blit_pass::Blit_pass(Deferred_renderer& renderer,
	                     ecs::Entity_manager&,
	                     util::maybe<Meta_system&>,
	                     graphic::Texture_2D& src)
	  : _renderer(renderer)
	  , _src(src)
	  , _sampler(renderer.device().create_sampler(1,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eLinear,
	                                              vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(renderer.device(), *_sampler, 3)
	  , _descriptor_set(_descriptor_set_layout.create_set(
	            renderer.descriptor_pool(),
	            {src.view(),
	             renderer.gbuffer().avg_log_luminance.get_or(src).view(),
	             renderer.gbuffer().bloom.get_or(src).view()}))
	  , _render_pass(build_render_pass(renderer, *_descriptor_set_layout, _framebuffers))
	  , _tone_mapping_enabled(renderer.gbuffer().avg_log_luminance.is_some())
	  , _bloom_enabled(renderer.gbuffer().bloom.is_some()) {}


	void Blit_pass::update(util::Time dt) {}

	void Blit_pass::draw(vk::CommandBuffer& command_buffer,
	                     Command_buffer_source&,
	                     vk::DescriptorSet global_uniform_set,
	                     std::size_t       swapchain_image) {

		_render_pass.execute(command_buffer, _framebuffers.at(swapchain_image), [&] {
			auto descriptor_sets =
			        std::array<vk::DescriptorSet, 2>{global_uniform_set, *_descriptor_set};
			_render_pass.bind_descriptor_sets(0, descriptor_sets);

			glm::vec4 settings;
			settings.x = _tone_mapping_enabled ? 1 : 0;
			settings.y = _bloom_enabled && _renderer.settings().bloom ? 20 : 0;
			settings.z = _renderer.settings().exposure_override;

			_render_pass.push_constant("settings"_strid, settings);

			command_buffer.draw(3, 1, 0, 0);
		});
	}


	auto Blit_pass_factory::create_pass(Deferred_renderer&        renderer,
	                                    ecs::Entity_manager&      entities,
	                                    util::maybe<Meta_system&> meta_system,
	                                    bool& write_first_pp_buffer) -> std::unique_ptr<Pass> {
		auto& color_src =
		        !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		return std::make_unique<Blit_pass>(renderer, entities, meta_system, color_src);
	}

	auto Blit_pass_factory::rank_device(vk::PhysicalDevice,
	                                    util::maybe<std::uint32_t> graphics_queue,
	                                    int                        current_score) -> int {
		return current_score;
	}

	void Blit_pass_factory::configure_device(vk::PhysicalDevice,
	                                         util::maybe<std::uint32_t>,
	                                         graphic::Device_create_info&) {}
} // namespace mirrage::renderer
