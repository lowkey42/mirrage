#include <mirrage/renderer/pass/tone_mapping_pass.hpp>

#include <bitset>

namespace mirrage::renderer {

	namespace {
		struct Push_constants {
			glm::vec4 parameters;
		};

		constexpr auto histogram_slots = 256;
		constexpr auto histogram_buffer_length =
		        histogram_slots + 4; // bins, display_factor, display_offset, La, p(La)
		constexpr auto histogram_buffer_size = histogram_buffer_length * sizeof(float);
		static_assert(sizeof(float) == sizeof(std::uint32_t));
		constexpr auto workgroup_size       = 32;
		constexpr auto histogram_batch_size = 8;
		constexpr auto histogram_host_visible =
#ifdef HPC_HISTOGRAM_DEBUG_VIEW
		        true;
#else
		        false;
#endif

		auto build_veil_render_pass(Deferred_renderer&                 renderer,
		                            vk::DescriptorSetLayout            desc_set_layout,
		                            graphic::Render_target_2D&         target,
		                            std::vector<graphic::Framebuffer>& out_framebuffers)
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

			pipeline.add_push_constant(
			        "pcs"_strid, sizeof(Push_constants), vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(screen);

			pass.stage("veil"_strid)
			        .shader("frag_shader:tone_mapping_veil"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:tone_mapping_veil"_aid, graphic::Shader_stage::vertex);

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

			out_framebuffers = util::build_vector(target.mip_levels(), [&](auto i) {
				return builder.build_framebuffer(
				        {target.view(i), util::Rgba{}}, target.width(i), target.height(i));
				;
			});

			return render_pass;
		}

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

			pipeline.add_push_constant(
			        "pcs"_strid, sizeof(Push_constants), vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(screen);

			pass.stage("apply"_strid)
			        .shader("frag_shader:tone_mapping_apply"_aid,
			                graphic::Shader_stage::fragment,
			                "main",
			                0,
			                std::log(renderer.gbuffer().min_luminance),
			                1,
			                std::log(renderer.gbuffer().max_luminance),
			                2,
			                histogram_slots)
			        .shader("vert_shader:tone_mapping_apply"_aid, graphic::Shader_stage::vertex);

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

		auto build_compute_pipeline_layout(Deferred_renderer&             renderer,
		                                   const vk::DescriptorSetLayout& desc_set_layout)
		{
			auto dsl = std::array<vk::DescriptorSetLayout, 2>{renderer.global_uniforms_layout(),
			                                                  desc_set_layout};
			auto pcs = vk::PushConstantRange{vk::ShaderStageFlagBits::eCompute, 0, sizeof(Push_constants)};

			return renderer.device().vk_device()->createPipelineLayoutUnique(
			        vk::PipelineLayoutCreateInfo{{}, dsl.size(), dsl.data(), 1, &pcs});
		}
		auto build_compute_pipeline(graphic::Device&      device,
		                            asset::Asset_manager& assets,
		                            vk::PipelineLayout    layout,
		                            float                 histogram_min,
		                            float                 histogram_max,
		                            const asset::AID&     shader)
		{
			auto module = assets.load<graphic::Shader_module>(shader);

			auto spec_entries =
			        std::array<vk::SpecializationMapEntry, 4>{vk::SpecializationMapEntry{0, 0 * 32, 32},
			                                                  vk::SpecializationMapEntry{1, 1 * 32, 32},
			                                                  vk::SpecializationMapEntry{2, 2 * 32, 32},
			                                                  vk::SpecializationMapEntry{3, 3 * 32, 32}};
			auto spec_data                                     = std::array<char, 4 * 32>();
			reinterpret_cast<std::int32_t&>(spec_data[0 * 32]) = histogram_slots;
			reinterpret_cast<std::int32_t&>(spec_data[1 * 32]) = workgroup_size;
			reinterpret_cast<float&>(spec_data[2 * 32])        = std::log(histogram_min);
			reinterpret_cast<float&>(spec_data[3 * 32])        = std::log(histogram_max);

			auto spec_info = vk::SpecializationInfo{
			        spec_entries.size(), spec_entries.data(), spec_data.size(), spec_data.data()};

			auto stage = vk::PipelineShaderStageCreateInfo{
			        {}, vk::ShaderStageFlagBits::eCompute, **module, "main", &spec_info};

			return device.vk_device()->createComputePipelineUnique(
			        device.pipeline_cache(), vk::ComputePipelineCreateInfo{{}, stage, layout});
		}

		auto get_luminance_format(graphic::Device& device)
		{
			auto format = device.get_supported_format(
			        {vk::Format::eR16Sfloat},
			        vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eStorageImage
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			MIRRAGE_INVARIANT(format.is_some(), "No Float R16 format supported (required for tone mapping)!");

			return format.get_or_throw();
		}

		auto compute_foveal_mip_level(float base_height, float v_fov)
		{
			auto target_height = static_cast<int>(std::round(2.f * std::tan(v_fov / 2.f) / 0.01745f));

			return static_cast<std::uint32_t>(std::round(glm::log2(base_height / target_height)));
		}

	} // namespace


	Tone_mapping_pass::Tone_mapping_pass(Deferred_renderer& renderer,
	                                     ecs::Entity_manager&,
	                                     util::maybe<Meta_system&>,
	                                     graphic::Render_target_2D& src,
	                                     graphic::Render_target_2D& target)
	  : _renderer(renderer)
	  , _src(src)
	  , _target(target)

	  , _last_result_data(histogram_buffer_length, 0.f)

	  , _sampler(renderer.device().create_sampler(1,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eLinear,
	                                              vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(renderer.device(), *_sampler, 3)
	  , _luminance_format(get_luminance_format(renderer.device()))

	  , _adjustment_buffer(renderer.device(),
	                       {histogram_slots, 2},
	                       1,
	                       _luminance_format,
	                       vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
	                               | vk::ImageUsageFlagBits::eColorAttachment
	                               | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
	                       vk::ImageAspectFlagBits::eColor)

	  , _compute_descriptor_set_layout(renderer.device().create_descriptor_set_layout(std::vector{
	            vk::DescriptorSetLayoutBinding(
	                    0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
	            vk::DescriptorSetLayoutBinding(
	                    1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
	            vk::DescriptorSetLayoutBinding(
	                    2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
	            vk::DescriptorSetLayoutBinding(
	                    3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
	            vk::DescriptorSetLayoutBinding(
	                    4, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute)}))

	  , _veil_buffer(renderer.device(),
	                 {renderer.gbuffer().colorA.width(), renderer.gbuffer().colorA.height()},
	                 0,
	                 renderer.gbuffer().color_format,
	                 vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
	                         | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                         | vk::ImageUsageFlagBits::eStorage,
	                 vk::ImageAspectFlagBits::eColor)

	  , _veil_renderpass(
	            build_veil_render_pass(renderer, *_descriptor_set_layout, _veil_buffer, _veil_framebuffers))
	  , _veil_desc_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                                     {src.view(), src.view(), src.view()}))

	  , _compute_pipeline_layout(build_compute_pipeline_layout(_renderer, *_compute_descriptor_set_layout))
	  , _build_histogram_pipeline(build_compute_pipeline(_renderer.device(),
	                                                     renderer.asset_manager(),
	                                                     *_compute_pipeline_layout,
	                                                     renderer.gbuffer().min_luminance,
	                                                     renderer.gbuffer().max_luminance,
	                                                     "comp_shader:tone_mapping_histogram"_aid))

	  , _adjust_histogram_pipeline(build_compute_pipeline(_renderer.device(),
	                                                      renderer.asset_manager(),
	                                                      *_compute_pipeline_layout,
	                                                      renderer.gbuffer().min_luminance,
	                                                      renderer.gbuffer().max_luminance,
	                                                      "comp_shader:tone_mapping_adjust"_aid))

	  , _apply_renderpass(
	            build_apply_render_pass(renderer, *_descriptor_set_layout, target, _apply_framebuffer))
	{

		_result_buffer = util::build_vector(renderer.device().max_frames_in_flight() + 1, [&](auto) {
			return graphic::Backed_buffer{renderer.device().create_buffer(
			        vk::BufferCreateInfo{
			                {},
			                histogram_buffer_size,
			                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst},
			        histogram_host_visible,
			        graphic::Memory_lifetime::persistent)};
		});

		_apply_desc_sets = util::build_vector(src.mip_levels(), [&](auto i) {
			return _descriptor_set_layout.create_set(
			        renderer.descriptor_pool(),
			        {src.view(0), _adjustment_buffer.view(0), _veil_buffer.view(i)});
		});

		_compute_descriptor_set = util::build_vector(_result_buffer.size() * src.mip_levels(), [&](auto) {
			return graphic::DescriptorSet{
			        renderer.descriptor_pool().create_descriptor(*_compute_descriptor_set_layout, 5)};
		});

		auto comp_desc_final =
		        vk::DescriptorImageInfo{*_sampler, _adjustment_buffer.view(0), vk::ImageLayout::eGeneral};

		auto comp_desc_buffers = util::build_vector(_result_buffer.size(), [&](auto i) {
			return vk::DescriptorBufferInfo{*_result_buffer[i], 0, VK_WHOLE_SIZE};
		});


		for(auto mip : util::range(src.mip_levels())) {
			auto comp_desc_writes = std::vector<vk::WriteDescriptorSet>();
			comp_desc_writes.reserve(_result_buffer.size() * 3);

			auto comp_desc_image =
			        vk::DescriptorImageInfo{*_sampler, _src.view(mip), vk::ImageLayout::eGeneral};

			auto comp_desc_veil =
			        vk::DescriptorImageInfo{*_sampler, _veil_buffer.view(mip), vk::ImageLayout::eGeneral};

			for(auto i : util::range(_result_buffer.size())) {
				comp_desc_writes.emplace_back(*_compute_descriptor_set[i * src.mip_levels() + mip],
				                              0,
				                              0,
				                              1,
				                              vk::DescriptorType::eStorageImage,
				                              &comp_desc_image);

				comp_desc_writes.emplace_back(*_compute_descriptor_set[i * src.mip_levels() + mip],
				                              1,
				                              0,
				                              1,
				                              vk::DescriptorType::eStorageBuffer,
				                              nullptr,
				                              &comp_desc_buffers[i]);

				comp_desc_writes.emplace_back(*_compute_descriptor_set[i * src.mip_levels() + mip],
				                              2,
				                              0,
				                              1,
				                              vk::DescriptorType::eStorageImage,
				                              &comp_desc_final);

				auto prev_buffer = i > 0 ? i - 1 : _result_buffer.size() - 1;
				comp_desc_writes.emplace_back(*_compute_descriptor_set[i * src.mip_levels() + mip],
				                              3,
				                              0,
				                              1,
				                              vk::DescriptorType::eStorageBuffer,
				                              nullptr,
				                              &comp_desc_buffers[prev_buffer]);

				comp_desc_writes.emplace_back(*_compute_descriptor_set[i * src.mip_levels() + mip],
				                              4,
				                              0,
				                              1,
				                              vk::DescriptorType::eStorageImage,
				                              &comp_desc_veil);
			}

			renderer.device().vk_device()->updateDescriptorSets(
			        gsl::narrow<std::uint32_t>(comp_desc_writes.size()), comp_desc_writes.data(), 0, nullptr);
		}

		renderer.gbuffer().histogram_adjustment_factors = _adjustment_buffer;
	}


	void Tone_mapping_pass::update(util::Time dt) {}

	void Tone_mapping_pass::draw(vk::CommandBuffer& command_buffer,
	                             Command_buffer_source&,
	                             vk::DescriptorSet global_uniform_set,
	                             std::size_t)
	{
		if(_first_frame) {
			_first_frame = false;
			// clear all internal storage on first exec
			graphic::clear_texture(command_buffer,
			                       _adjustment_buffer,
			                       {0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal,
			                       0,
			                       1);

			for(auto& buffer : _result_buffer) {
				command_buffer.fillBuffer(*buffer, 0, VK_WHOLE_SIZE, 0);
			}
		}

		if(histogram_host_visible && _ready_result >= 0) {
			// read back histogram + exposure
			auto data_addr = _result_buffer[_ready_result].memory().mapped_addr().get_or_throw(
			        "Host visible buffer is not mapped!");

			auto data = reinterpret_cast<std::uint32_t*>(data_addr);

			for(auto i : util::range(_last_result_data.size())) {
				_last_result_data[i] = data[i];
			}

			for(auto i : util::range(1, 4)) {
				_last_result_data[_last_result_data.size() - i] =
				        *(reinterpret_cast<float*>(data_addr) + (histogram_buffer_length - i));
			}
		}

		_clear_result_buffer(command_buffer);

		auto foveal_mip_level = _generate_foveal_image(command_buffer);
		_generate_veil_image(global_uniform_set, command_buffer, foveal_mip_level);
		_dispatch_build_histogram(global_uniform_set, command_buffer, foveal_mip_level);
		_dispatch_adjust_histogram(global_uniform_set, command_buffer, foveal_mip_level);
		_apply_tone_ampping(global_uniform_set, command_buffer, foveal_mip_level);


		// increment index of next/ready result
		_next_result++;
		if(_ready_result >= 0) {
			_ready_result = (_ready_result + 1) % static_cast<int>(_result_buffer.size());
		}
		if(_next_result >= static_cast<int>(_result_buffer.size())) {
			_next_result  = 0;
			_ready_result = 1;
		}
	}

	void Tone_mapping_pass::_clear_result_buffer(vk::CommandBuffer& command_buffer)
	{
		auto acquire =
		        vk::BufferMemoryBarrier{vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		                                vk::AccessFlagBits::eTransferWrite,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                *_result_buffer[_next_result],
		                                0,
		                                VK_WHOLE_SIZE};

		command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
		                               vk::PipelineStageFlagBits::eTransfer,
		                               vk::DependencyFlags{},
		                               {},
		                               {acquire},
		                               {});

		command_buffer.fillBuffer(*_result_buffer[_next_result], 0, VK_WHOLE_SIZE, 0);

		auto release =
		        vk::BufferMemoryBarrier{vk::AccessFlagBits::eTransferWrite,
		                                vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                *_result_buffer[_next_result],
		                                0,
		                                VK_WHOLE_SIZE};

		command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
		                               vk::PipelineStageFlagBits::eComputeShader,
		                               vk::DependencyFlags{},
		                               {},
		                               {release},
		                               {});
	}
	auto Tone_mapping_pass::_generate_foveal_image(vk::CommandBuffer& command_buffer) -> std::uint32_t
	{
		auto _ = _renderer.profiler().push("Foveal rescale");

		auto foveal_mip_level =
		        compute_foveal_mip_level(_src.height(), _renderer.global_uniforms().proj_planes.w);
		_last_max_histogram_size = _src.height(foveal_mip_level) * _src.width(foveal_mip_level);
		if(foveal_mip_level > 0) {
			graphic::generate_mipmaps(command_buffer,
			                          _src.image(),
			                          vk::ImageLayout::eShaderReadOnlyOptimal,
			                          vk::ImageLayout::eShaderReadOnlyOptimal,
			                          _src.width(),
			                          _src.height(),
			                          foveal_mip_level + 1,
			                          0);
		}

		return foveal_mip_level;
	}

	void Tone_mapping_pass::_generate_veil_image(vk::DescriptorSet  global_uniform_set,
	                                             vk::CommandBuffer& command_buffer,
	                                             std::uint32_t      mip_level)
	{
		auto _ = _renderer.profiler().push("Gen. Veil Image");

		_veil_renderpass.execute(command_buffer, _veil_framebuffers.at(mip_level), [&] {
			auto desc_sets = std::array<vk::DescriptorSet, 2>{global_uniform_set, *_veil_desc_set};
			_veil_renderpass.bind_descriptor_sets(0, desc_sets);
			command_buffer.draw(3, 1, 0, 0);
		});
	}

	void Tone_mapping_pass::_dispatch_build_histogram(vk::DescriptorSet  global_uniform_set,
	                                                  vk::CommandBuffer& command_buffer,
	                                                  std::uint32_t      mip_level)
	{
		auto _ = _renderer.profiler().push("Build Histogram");

		// barriers and layout transfers of inputs
		auto acquire_input = vk::ImageMemoryBarrier{
		        vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eColorAttachmentWrite,
		        vk::AccessFlagBits::eShaderRead,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        vk::ImageLayout::eGeneral,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
		        _src.image(),
		        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, mip_level, 1, 0, 1}};

		auto acquire_veil = vk::ImageMemoryBarrier{
		        vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eColorAttachmentWrite,
		        vk::AccessFlagBits::eShaderRead,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        vk::ImageLayout::eGeneral,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
		        _veil_buffer.image(),
		        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, mip_level, 1, 0, 1}};

		command_buffer.pipelineBarrier(
		        vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eColorAttachmentOutput,
		        vk::PipelineStageFlagBits::eComputeShader,
		        vk::DependencyFlags{},
		        {},
		        {},
		        {acquire_input, acquire_veil});


		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *_build_histogram_pipeline);

		auto desc_sets = std::array<vk::DescriptorSet, 2>{
		        global_uniform_set, *_compute_descriptor_set[_next_result * _src.mip_levels() + mip_level]};
		command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
		                                  *_compute_pipeline_layout,
		                                  0,
		                                  desc_sets.size(),
		                                  desc_sets.data(),
		                                  0,
		                                  nullptr);

		command_buffer.dispatch(
		        static_cast<std::uint32_t>(
		                std::ceil(_src.width(mip_level) / float(workgroup_size * histogram_batch_size))),
		        static_cast<std::uint32_t>(
		                std::ceil(_src.height(mip_level) / float(workgroup_size * histogram_batch_size))),
		        1);
	}

	void Tone_mapping_pass::_dispatch_adjust_histogram(vk::DescriptorSet  global_uniform_set,
	                                                   vk::CommandBuffer& command_buffer,
	                                                   std::uint32_t      mip_level)
	{
		auto _ = _renderer.profiler().push("Adjust Histogram");

		auto acquire_prev_result = vk::BufferMemoryBarrier{
		        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
		        *_result_buffer[_next_result > 0 ? _next_result - 1 : _result_buffer.size() - 1],
		        0,
		        VK_WHOLE_SIZE};

		auto acquire_result =
		        vk::BufferMemoryBarrier{vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		                                vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                VK_QUEUE_FAMILY_IGNORED,
		                                *_result_buffer[_next_result],
		                                0,
		                                VK_WHOLE_SIZE};

		command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
		                               vk::PipelineStageFlagBits::eComputeShader,
		                               vk::DependencyFlags{},
		                               {},
		                               {acquire_result, acquire_prev_result},
		                               {});

		auto acquire_adjustment_buffer = vk::ImageMemoryBarrier{
		        vk::AccessFlagBits::eShaderRead,
		        vk::AccessFlagBits::eShaderWrite,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        vk::ImageLayout::eGeneral,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
		        _adjustment_buffer.image(),
		        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

		command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
		                               vk::PipelineStageFlagBits::eComputeShader,
		                               vk::DependencyFlags{},
		                               {},
		                               {},
		                               {acquire_adjustment_buffer});

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *_adjust_histogram_pipeline);

		auto desc_sets = std::array<vk::DescriptorSet, 2>{
		        global_uniform_set, *_compute_descriptor_set[_next_result * _src.mip_levels() + mip_level]};
		command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
		                                  *_compute_pipeline_layout,
		                                  0,
		                                  desc_sets.size(),
		                                  desc_sets.data(),
		                                  0,
		                                  nullptr);

		auto pcs         = Push_constants{};
		pcs.parameters.x = std::log(std::max(_renderer.settings().min_display_luminance, 0.0001f));
		pcs.parameters.y = std::log(_renderer.settings().max_display_luminance);
		command_buffer.pushConstants(
		        *_compute_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(Push_constants), &pcs);

		command_buffer.dispatch(1, 1, 1);
	}

	void Tone_mapping_pass::_apply_tone_ampping(vk::DescriptorSet  global_uniform_set,
	                                            vk::CommandBuffer& command_buffer,
	                                            std::uint32_t      mip_level)
	{
		auto _ = _renderer.profiler().push("Apply");

		// transfer texture layout back to fragment shader readable
		auto acquire_input = vk::ImageMemoryBarrier{
		        vk::AccessFlagBits::eShaderRead,
		        vk::AccessFlagBits::eShaderRead,
		        vk::ImageLayout::eGeneral,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
		        _src.image(),
		        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, mip_level, 1, 0, 1}};

		auto acquire_adjustment_buffer = vk::ImageMemoryBarrier{
		        vk::AccessFlagBits::eShaderWrite,
		        vk::AccessFlagBits::eShaderRead,
		        vk::ImageLayout::eGeneral,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
		        _adjustment_buffer.image(),
		        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

		command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
		                               vk::PipelineStageFlagBits::eFragmentShader,
		                               vk::DependencyFlags{},
		                               {},
		                               {},
		                               {acquire_adjustment_buffer, acquire_input});

		_apply_renderpass.execute(command_buffer, _apply_framebuffer, [&] {
			auto desc_sets =
			        std::array<vk::DescriptorSet, 2>{global_uniform_set, *_apply_desc_sets.at(mip_level)};
			_apply_renderpass.bind_descriptor_sets(0, desc_sets);

			command_buffer.draw(3, 1, 0, 0);
		});
	}

	auto Tone_mapping_pass_factory::create_pass(Deferred_renderer&        renderer,
	                                            ecs::Entity_manager&      entities,
	                                            util::maybe<Meta_system&> meta_system,
	                                            bool& write_first_pp_buffer) -> std::unique_ptr<Pass>
	{
		auto& color_src  = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;
		auto& color_dest = !write_first_pp_buffer ? renderer.gbuffer().colorB : renderer.gbuffer().colorA;

		write_first_pp_buffer = !write_first_pp_buffer;

		return std::make_unique<Tone_mapping_pass>(renderer, entities, meta_system, color_src, color_dest);
	}

	auto Tone_mapping_pass_factory::rank_device(vk::PhysicalDevice,
	                                            util::maybe<std::uint32_t> graphics_queue,
	                                            int                        current_score) -> int
	{
		return current_score;
	}

	void Tone_mapping_pass_factory::configure_device(vk::PhysicalDevice,
	                                                 util::maybe<std::uint32_t>,
	                                                 graphic::Device_create_info& create_info)
	{
		create_info.features.shaderStorageImageExtendedFormats = true;
	}
} // namespace mirrage::renderer
