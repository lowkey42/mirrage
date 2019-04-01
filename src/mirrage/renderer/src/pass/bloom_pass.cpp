#include <mirrage/renderer/pass/bloom_pass.hpp>


namespace mirrage::renderer {

	namespace {
		struct Push_constants {
			glm::vec4 parameters; // threshold
		};

		auto build_apply_render_pass(Deferred_renderer&         renderer,
		                             vk::DescriptorSetLayout    desc_set_layout,
		                             graphic::Render_target_2D& target,
		                             graphic::Framebuffer&      out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto screen = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eLoad,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant(
			        "pcs"_strid, sizeof(Push_constants), vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        screen, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			pass.stage("apply"_strid)
			        .shader("frag_shader:bloom_apply"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:bloom_apply"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer(
			        {target.view(0), util::Rgba{}}, target.width(), target.height());

			return render_pass;
		}

		auto build_blur_render_pass(Deferred_renderer&                 renderer,
		                            vk::DescriptorSetLayout            desc_set_layout,
		                            graphic::Render_target_2D&         target_horizontal,
		                            graphic::Render_target_2D&         target_vertical,
		                            std::vector<graphic::Framebuffer>& out_framebuffer_horizontal,
		                            std::vector<graphic::Framebuffer>& out_framebuffer_vertical)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto screen = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);

			auto& pass = builder.add_subpass(pipeline).color_attachment(screen);

			pass.stage("blur_h"_strid)
			        .shader("frag_shader:bloom_blur"_aid, graphic::Shader_stage::fragment, "main", 0, 1)
			        .shader("vert_shader:bloom_blur"_aid, graphic::Shader_stage::vertex, "main", 0, 1);

			pass.stage("blur_v"_strid)
			        .shader("frag_shader:bloom_blur"_aid, graphic::Shader_stage::fragment, "main", 0, 0)
			        .shader("vert_shader:bloom_blur"_aid, graphic::Shader_stage::vertex, "main", 0, 0);

			pass.stage("blur_v_last"_strid)
			        .shader("frag_shader:bloom_blur"_aid, graphic::Shader_stage::fragment, "main", 0, 0, 1, 1)
			        .shader("vert_shader:bloom_blur"_aid, graphic::Shader_stage::vertex, "main", 0, 0);

			auto render_pass = builder.build();

			out_framebuffer_horizontal = util::build_vector(target_horizontal.mip_levels(), [&](auto i) {
				return builder.build_framebuffer({target_horizontal.view(i), util::Rgba{}},
				                                 target_horizontal.width(i),
				                                 target_horizontal.height(i));
			});

			out_framebuffer_vertical = util::build_vector(target_vertical.mip_levels(), [&](auto i) {
				return builder.build_framebuffer({target_vertical.view(i), util::Rgba{}},
				                                 target_vertical.width(i),
				                                 target_vertical.height(i));
			});

			return render_pass;
		}
	} // namespace


	Bloom_pass::Bloom_pass(Deferred_renderer& renderer, graphic::Render_target_2D& src)
	  : _renderer(renderer)
	  , _src(src)
	  , _sampler(renderer.device().create_sampler(12,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eLinear,
	                                              vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(renderer.device(),
	                           *_sampler,
	                           2,
	                           vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex)

	  , _bloom_buffer(renderer.device(),
	                  {src.width(0), src.height(0)},
	                  0,
	                  renderer.gbuffer().color_format,
	                  vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
	                          | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	                  vk::ImageAspectFlagBits::eColor)
	  , _apply_renderpass(build_apply_render_pass(renderer, *_descriptor_set_layout, src, _apply_framebuffer))
	  , _apply_descriptor_set(
	            _descriptor_set_layout.create_set(_renderer.descriptor_pool(), {_bloom_buffer.view()}))

	  , _blur_buffer(renderer.device(),
	                 {_bloom_buffer.width(), _bloom_buffer.height()},
	                 0,
	                 renderer.gbuffer().color_format,
	                 vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
	                         | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	                 vk::ImageAspectFlagBits::eColor)
	  , _blur_renderpass(build_blur_render_pass(renderer,
	                                            *_descriptor_set_layout,
	                                            _blur_buffer,
	                                            _bloom_buffer,
	                                            _blur_framebuffer_horizontal,
	                                            _blur_framebuffer_vertical))

	  , _blur_descriptor_set_horizontal(
	            _descriptor_set_layout.create_set(renderer.descriptor_pool(), {_src.view(), _src.view()}))
	  , _blur_descriptor_set_vertical(_descriptor_set_layout.create_set(
	            renderer.descriptor_pool(), {_blur_buffer.view(), _blur_buffer.view()}))
	{

		_downsampled_blur_views = util::build_vector(_blur_buffer.mip_levels() - 1, [&](auto i) {
			return renderer.device().create_image_view(
			        _bloom_buffer.image(), renderer.gbuffer().color_format, i + 1, VK_REMAINING_MIP_LEVELS);
		});

		_blur_descriptor_set_vertical_final = util::build_vector(_blur_buffer.mip_levels() - 1, [&](auto i) {
			return _descriptor_set_layout.create_set(
			        renderer.descriptor_pool(),
			        {_blur_buffer.view(), *_downsampled_blur_views.at(std::size_t(i))});
		});
	}


	void Bloom_pass::update(util::Time) {}

	void Bloom_pass::draw(Frame_data& frame)
	{
		if(_first_frame) {
			_first_frame = false;

			graphic::clear_texture(frame.main_command_buffer,
			                       _bloom_buffer,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal);
			graphic::clear_texture(frame.main_command_buffer,
			                       _blur_buffer,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal);
			return;
		}

		if(!_renderer.settings().bloom)
			return;

		auto blur_mip_levels = 2;
		auto start_mip_level = std::min(3, _src.mip_levels() - blur_mip_levels);


		auto pcs = Push_constants{};

		// generate mip chain
		graphic::generate_mipmaps(frame.main_command_buffer,
		                          _src.image(),
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          _src.width(),
		                          _src.height(),
		                          start_mip_level + blur_mip_levels);

		auto start       = std::min(blur_mip_levels + start_mip_level - 1, _blur_buffer.mip_levels() - 1);
		pcs.parameters.y = float(start - start_mip_level);

		for(auto i = start; i >= start_mip_level; i--) {
			pcs.parameters.x = float(i - 1);

			// blur horizontal
			_blur_renderpass.execute(
			        frame.main_command_buffer, _blur_framebuffer_horizontal.at(std::size_t(i - 1)), [&] {
				        _blur_renderpass.bind_descriptor_set(1, *_blur_descriptor_set_horizontal);

				        _blur_renderpass.push_constant("pcs"_strid, pcs);

				        frame.main_command_buffer.draw(3, 1, 0, 0);
			        });

			// blur vertical
			_blur_renderpass.execute(
			        frame.main_command_buffer, _blur_framebuffer_vertical.at(std::size_t(i - 1)), [&] {
				        if(i < start) {
					        _blur_renderpass.set_stage("blur_v_last"_strid);
					        _blur_renderpass.bind_descriptor_set(
					                1, *_blur_descriptor_set_vertical_final.at(std::size_t(i)));
				        } else {
					        _blur_renderpass.set_stage("blur_v"_strid);
					        _blur_renderpass.bind_descriptor_set(1, *_blur_descriptor_set_vertical);
				        }

				        _blur_renderpass.push_constant("pcs"_strid, pcs);

				        frame.main_command_buffer.draw(3, 1, 0, 0);
			        });
		}

		// apply
		_apply_renderpass.execute(frame.main_command_buffer, _apply_framebuffer, [&] {
			auto descriptor_sets =
			        std::array<vk::DescriptorSet, 2>{frame.global_uniform_set, *_apply_descriptor_set};
			_apply_renderpass.bind_descriptor_sets(0, descriptor_sets);

			pcs.parameters.x = float(start_mip_level - 1);
			_apply_renderpass.push_constant("pcs"_strid, pcs);

			frame.main_command_buffer.draw(3, 1, 0, 0);
		});
	}


	auto Bloom_pass_factory::create_pass(Deferred_renderer& renderer,
	                                     std::shared_ptr<void>,
	                                     util::maybe<ecs::Entity_manager&>,
	                                     Engine&,
	                                     bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		if(!renderer.settings().bloom)
			return {};

		auto& src = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		return std::make_unique<Bloom_pass>(renderer, src);
	}

	auto Bloom_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Bloom_pass_factory::configure_device(vk::PhysicalDevice,
	                                          util::maybe<std::uint32_t>,
	                                          graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
