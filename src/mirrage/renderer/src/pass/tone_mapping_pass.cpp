#include <mirrage/renderer/pass/tone_mapping_pass.hpp>


namespace mirrage {
namespace renderer {

	namespace {
		struct Push_constants {
			glm::vec4 parameters; // tau (controls adjustment speed), active sampler
			// TODO
		};

		auto build_luminance_render_pass(Deferred_renderer& renderer,
		                                 vk::DescriptorSetLayout desc_set_layout,
		                                 vk::Format luminance_format,
		                                 graphic::Render_target_2D& target,
		                                 graphic::Framebuffer& out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto screen = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				luminance_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferSrcOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid, sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(screen);

			pass.stage("lum"_strid)
			    .shader("frag_shader:luminance"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:luminance"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer({target.view(0), util::Rgba{}},
				                                        target.width(),
				                                        target.height());

			return render_pass;
		}

		auto build_adapt_render_pass(Deferred_renderer& renderer,
		                             vk::DescriptorSetLayout desc_set_layout,
		                             vk::Format luminance_format,
		                             graphic::Render_target_2D& target,
		                             graphic::Framebuffer& out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto screen = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				luminance_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid, sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(screen);

			pass.stage("adapt"_strid)
			    .shader("frag_shader:luminance_adapt"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:luminance_adapt"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);

			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer({target.view(0), util::Rgba{}},
			                                            target.width(),
			                                            target.height());

			return render_pass;
		}

		auto get_luminance_format(graphic::Device& device) {
			auto format = device.get_supported_format({vk::Format::eR32Sfloat},
			                                          vk::FormatFeatureFlagBits::eColorAttachment
			                                          | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			INVARIANT(format.is_some(), "No Float R16 format supported (required for tone mapping)!");

			return format.get_or_throw();
		}
	}


	Tone_mapping_pass::Tone_mapping_pass(Deferred_renderer& renderer,
	                                     ecs::Entity_manager&,
	                                     util::maybe<Meta_system&>,
	                                     graphic::Texture_2D& src)
	    : _renderer(renderer)
	    , _sampler(renderer.device().create_sampler(1, vk::SamplerAddressMode::eClampToEdge,
	                                                vk::BorderColor::eIntOpaqueBlack,
	                                                vk::Filter::eLinear,
	                                                vk::SamplerMipmapMode::eNearest))
	    , _descriptor_set_layout(renderer.device(), *_sampler, 2)
	    , _luminance_format(get_luminance_format(renderer.device()))

	    , _luminance_buffer(renderer.device(),
	                        {1024, 1024},
	                        0,
	                        _luminance_format,
	                        vk::ImageUsageFlagBits::eTransferSrc
	                        | vk::ImageUsageFlagBits::eTransferDst
	                        | vk::ImageUsageFlagBits::eColorAttachment
	                        | vk::ImageUsageFlagBits::eSampled,
	                        vk::ImageAspectFlagBits::eColor)
	    , _calc_luminance_renderpass(build_luminance_render_pass(renderer, *_descriptor_set_layout,
	                                                             _luminance_format,
	                                                             _luminance_buffer,
	                                                             _calc_luminance_framebuffer))
	    , _calc_luminance_desc_set(_descriptor_set_layout.create_set(
	                          renderer.descriptor_pool(),
	                          {src.view(), src.view()}))

	    , _prev_avg_luminance(renderer.device(),
	                          {2, 2},
	                          1,
	                          _luminance_format,
	                          vk::ImageUsageFlagBits::eTransferSrc
	                          | vk::ImageUsageFlagBits::eTransferDst
	                          | vk::ImageUsageFlagBits::eColorAttachment
	                          | vk::ImageUsageFlagBits::eSampled,
	                          vk::ImageAspectFlagBits::eColor)
	    , _curr_avg_luminance(renderer.device(),
	                          {2, 2},
	                          1,
	                          _luminance_format,
	                          vk::ImageUsageFlagBits::eTransferSrc
	                          | vk::ImageUsageFlagBits::eTransferDst
	                          | vk::ImageUsageFlagBits::eColorAttachment
	                          | vk::ImageUsageFlagBits::eSampled,
	                          vk::ImageAspectFlagBits::eColor)

	    , _adapt_luminance_renderpass(build_adapt_render_pass(renderer, *_descriptor_set_layout,
	                                                          _luminance_format,
	                                                          _curr_avg_luminance,
	                                                          _adapt_luminance_framebuffer))
	    , _adapt_luminance_desc_set(_descriptor_set_layout.create_set(
	                                renderer.descriptor_pool(),
	                                {_luminance_buffer.view(_luminance_buffer.mip_levels()-1),
	                                 _prev_avg_luminance.view(0)})) {

		renderer.gbuffer().avg_log_luminance = _curr_avg_luminance;
	}


	void Tone_mapping_pass::update(util::Time dt) {
	}

	void Tone_mapping_pass::draw(vk::CommandBuffer& command_buffer,
	                     Command_buffer_source&,
	                     vk::DescriptorSet global_uniform_set,
	                     std::size_t) {

		auto pcs = Push_constants{};
		pcs.parameters.x = 0.06f; // TODO: make configurable
		pcs.parameters.y = 0.7f; // TODO: make configurable

		if(_first_frame) {
			_first_frame = false;

			graphic::image_layout_transition(command_buffer,
			                                 _prev_avg_luminance.image(),
			                                 vk::ImageLayout::eUndefined,
			                                 vk::ImageLayout::eTransferDstOptimal,
			                                 vk::ImageAspectFlagBits::eColor,
			                                 0, 1);

			command_buffer.clearColorImage(_prev_avg_luminance.image(),
			                               vk::ImageLayout::eTransferDstOptimal,
			                               vk::ClearColorValue{std::array<float,4>{0.f, 0.f, 0.f, 0.f}},
			                               {vk::ImageSubresourceRange{
			                                    vk::ImageAspectFlagBits::eColor,
			                                    0, 1,
			                                    0, 1
			                                }});

			graphic::image_layout_transition(command_buffer,
			                                 _prev_avg_luminance.image(),
			                                 vk::ImageLayout::eTransferDstOptimal,
			                                 vk::ImageLayout::eShaderReadOnlyOptimal,
			                                 vk::ImageAspectFlagBits::eColor,
			                                 0, 1);
		}

		// extract luminance
		_calc_luminance_renderpass.execute(command_buffer, _calc_luminance_framebuffer, [&] {
			auto descriptor_sets = std::array<vk::DescriptorSet, 2> {
				global_uniform_set,
				*_calc_luminance_desc_set
			};
			_calc_luminance_renderpass.bind_descriptor_sets(0, descriptor_sets);

			_calc_luminance_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});


		// generate mip chain to get avg luminance
		graphic::generate_mipmaps(command_buffer, _luminance_buffer.image(),
		                          vk::ImageLayout::eTransferSrcOptimal,
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          _luminance_buffer.width(), _luminance_buffer.height(), 0);

		// slowly adapt avg log exposure to current scene luminance
		_adapt_luminance_renderpass.execute(command_buffer, _adapt_luminance_framebuffer, [&] {
			_adapt_luminance_renderpass.bind_descriptor_set(1, *_adapt_luminance_desc_set);
			_adapt_luminance_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});

		// copy current to previous
		graphic::blit_texture(command_buffer, _curr_avg_luminance,
		                      vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
		                      _prev_avg_luminance,
		                      vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
	}


	auto Tone_mapping_pass_factory::create_pass(Deferred_renderer& renderer,
	                                    ecs::Entity_manager& entities,
	                                    util::maybe<Meta_system&> meta_system,
	                                    bool& write_first_pp_buffer) -> std::unique_ptr<Pass> {
		auto& color_src = !write_first_pp_buffer ? renderer.gbuffer().colorA
		                                         : renderer.gbuffer().colorB;
		
		return std::make_unique<Tone_mapping_pass>(renderer, entities, meta_system, color_src);
	}

	auto Tone_mapping_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
	                 int current_score) -> int {
		return current_score;
	}

	void Tone_mapping_pass_factory::configure_device(vk::PhysicalDevice,
	                                         util::maybe<std::uint32_t>,
	                                         graphic::Device_create_info&) {
	}


}
}
