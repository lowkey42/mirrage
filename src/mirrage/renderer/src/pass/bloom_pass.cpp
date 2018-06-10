#include <mirrage/renderer/pass/bloom_pass.hpp>


namespace mirrage::renderer {

	namespace {
		struct Push_constants {
			glm::vec4 parameters; // threshold
		};

		auto build_filter_render_pass(Deferred_renderer&         renderer,
		                              vk::DescriptorSetLayout    desc_set_layout,
		                              graphic::Render_target_2D& target,
		                              graphic::Framebuffer&      out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto screen =
			        builder.add_attachment(vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                                         renderer.gbuffer().color_format,
			                                                         vk::SampleCountFlagBits::e1,
			                                                         vk::AttachmentLoadOp::eDontCare,
			                                                         vk::AttachmentStoreOp::eStore,
			                                                         vk::AttachmentLoadOp::eDontCare,
			                                                         vk::AttachmentStoreOp::eDontCare,
			                                                         vk::ImageLayout::eUndefined,
			                                                         vk::ImageLayout::eTransferSrcOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant(
			        "pcs"_strid, sizeof(Push_constants), vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(screen);

			pass.stage("filter"_strid)
			        .shader("frag_shader:bloom_filter"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:bloom_filter"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(
			        util::nothing,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlags{},
			        pass,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

			builder.add_dependency(
			        pass,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
			        util::nothing,
			        vk::PipelineStageFlagBits::eBottomOfPipe,
			        vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eShaderRead
			                | vk::AccessFlagBits::eTransferRead);


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

			builder.add_dependency(
			        util::nothing,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlags{},
			        pass,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

			builder.add_dependency(
			        pass,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
			        util::nothing,
			        vk::PipelineStageFlagBits::eBottomOfPipe,
			        vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eShaderRead
			                | vk::AccessFlagBits::eTransferRead);


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

		auto compute_start_mip_level(float base_height, float v_fov)
		{
			auto target_height = static_cast<int>(std::round(2.f * std::tan(v_fov / 2.f) / 0.01745f));

			return static_cast<std::uint32_t>(std::round(glm::log2(base_height / target_height)));
		}

	} // namespace


	Bloom_pass::Bloom_pass(Deferred_renderer& renderer,
	                       ecs::Entity_manager&,
	                       util::maybe<Meta_system&>,
	                       graphic::Render_target_2D& src)
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
	                          | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                          | vk::ImageUsageFlagBits::eStorage,
	                  vk::ImageAspectFlagBits::eColor)
	  , _filter_renderpass(build_filter_render_pass(
	            renderer, *_descriptor_set_layout, _bloom_buffer, _filter_framebuffer))

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

	  , _blur_descriptor_set_horizontal(_descriptor_set_layout.create_set(
	            renderer.descriptor_pool(), {_bloom_buffer.view(), _bloom_buffer.view()}))
	  , _blur_descriptor_set_vertical(_descriptor_set_layout.create_set(
	            renderer.descriptor_pool(), {_blur_buffer.view(), _blur_buffer.view()}))
	{

		_downsampled_blur_views = util::build_vector(_blur_buffer.mip_levels() - 1, [&](auto i) {
			return renderer.device().create_image_view(
			        _bloom_buffer.image(), renderer.gbuffer().color_format, i + 1, VK_REMAINING_MIP_LEVELS);
		});

		_blur_descriptor_set_vertical_final = util::build_vector(_blur_buffer.mip_levels() - 1, [&](auto i) {
			return _descriptor_set_layout.create_set(renderer.descriptor_pool(),
			                                         {_blur_buffer.view(), *_downsampled_blur_views.at(i)});
		});


		renderer.gbuffer().bloom = _bloom_buffer;
	}


	void Bloom_pass::update(util::Time dt) {}

	void Bloom_pass::draw(vk::CommandBuffer& command_buffer,
	                      Command_buffer_source&,
	                      vk::DescriptorSet global_uniform_set,
	                      std::size_t)
	{
		if(_first_frame) {
			_first_frame = false;

			_filter_descriptor_set = _descriptor_set_layout.create_set(
			        _renderer.descriptor_pool(),
			        {_src.view(0),
			         _renderer.gbuffer()
			                 .avg_log_luminance.get_or_throw("Tonemapping pass is required for bloom!")
			                 .view()});

			graphic::clear_texture(command_buffer,
			                       _bloom_buffer,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal);
			return;
		}

		static constexpr auto blur_mip_levels = 4;
		auto                  start_mip_level =
		        compute_start_mip_level(_src.height(), _renderer.global_uniforms().proj_planes.w);
		start_mip_level = 1; //TODO


		auto pcs         = Push_constants{};
		pcs.parameters.x = 8.0f; // bloom threshold
		pcs.parameters.z = _renderer.settings().exposure_override;

		// filter
		_filter_renderpass.execute(command_buffer, _filter_framebuffer, [&] {
			auto descriptor_sets =
			        std::array<vk::DescriptorSet, 2>{global_uniform_set, *_filter_descriptor_set};
			_filter_renderpass.bind_descriptor_sets(0, descriptor_sets);

			_filter_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});

		// generate mip chain
		graphic::generate_mipmaps(command_buffer,
		                          _bloom_buffer.image(),
		                          vk::ImageLayout::eTransferSrcOptimal,
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          _bloom_buffer.width(),
		                          _bloom_buffer.height(),
		                          _bloom_buffer.mip_levels());

		auto start       = std::min(blur_mip_levels + start_mip_level - 1, _blur_buffer.mip_levels() - 1);
		pcs.parameters.y = start - start_mip_level;

		for(auto i = start; i >= start_mip_level; i--) {
			pcs.parameters.x = i - 1;

			// blur horizontal
			_blur_renderpass.execute(command_buffer, _blur_framebuffer_horizontal.at(i - 1), [&] {
				_blur_renderpass.bind_descriptor_set(1, *_blur_descriptor_set_horizontal);

				_blur_renderpass.push_constant("pcs"_strid, pcs);

				command_buffer.draw(3, 1, 0, 0);
			});

			// blur vertical
			_blur_renderpass.execute(command_buffer, _blur_framebuffer_vertical.at(i - 1), [&] {
				if(i == start_mip_level) {
					_blur_renderpass.set_stage("blur_v_last"_strid);
					_blur_renderpass.bind_descriptor_set(1, *_blur_descriptor_set_vertical_final.at(i));
				} else {
					_blur_renderpass.set_stage("blur_v"_strid);
					_blur_renderpass.bind_descriptor_set(1, *_blur_descriptor_set_vertical);
				}

				_blur_renderpass.push_constant("pcs"_strid, pcs);

				command_buffer.draw(3, 1, 0, 0);
			});
		}

		for(auto i = int(start_mip_level) - 1; i >= 1; i--) {
			graphic::blit_texture(command_buffer,
			                      _bloom_buffer,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _bloom_buffer,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      i,
			                      i - 1);
		}
	}


	auto Bloom_pass_factory::create_pass(Deferred_renderer&        renderer,
	                                     ecs::Entity_manager&      entities,
	                                     util::maybe<Meta_system&> meta_system,
	                                     bool& write_first_pp_buffer) -> std::unique_ptr<Pass>
	{
		auto& color_src = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		return std::make_unique<Bloom_pass>(renderer, entities, meta_system, color_src);
	}

	auto Bloom_pass_factory::rank_device(vk::PhysicalDevice,
	                                     util::maybe<std::uint32_t> graphics_queue,
	                                     int                        current_score) -> int
	{
		return current_score;
	}

	void Bloom_pass_factory::configure_device(vk::PhysicalDevice,
	                                          util::maybe<std::uint32_t>,
	                                          graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
