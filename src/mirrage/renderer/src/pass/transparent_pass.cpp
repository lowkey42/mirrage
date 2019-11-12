#include <mirrage/renderer/pass/transparent_pass.hpp>

#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/window.hpp>
#include <mirrage/utils/min_max.hpp>

#include <glm/gtx/string_cast.hpp>


namespace mirrage::renderer {

	using namespace util::unit_literals;

	namespace {
		struct Directional_light_uniforms {
			glm::mat4 light_space;
			glm::vec4 radiance;
			glm::vec4 shadow_radiance;
			glm::vec4 dir; // + int shadowmap;
		};

		struct Push_constants {
			glm::mat4 model;
			glm::vec4 light_color; //< for light-subpass; A=intensity
			glm::vec4 light_data;  //< for light-subpass; R=src_radius, GBA=direction
			glm::vec4 light_data2; //< for light-subpass; R=shadowmapID
			glm::vec4 shadow_color;
		};

		auto build_mip_render_pass(Deferred_renderer&                 renderer,
		                           vk::DescriptorSetLayout            desc_set_layout,
		                           std::vector<graphic::Framebuffer>& out_framebuffers)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto depth = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.device().get_depth_format(),
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eDepthStencilAttachmentOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil =
			        vk::PipelineDepthStencilStateCreateInfo({}, true, true, vk::CompareOp::eAlways);

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("dpc"_strid,
			                           sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			builder.add_subpass(pipeline)
			        .depth_stencil_attachment(depth)
			        .stage("depth_mipmap"_strid)
			        .shader("frag_shader:depth_mipgen"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:fullscreen"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			out_framebuffers.reserve(std::size_t(renderer.gbuffer().depth_buffer.mip_levels()));

			for(auto i : util::range(renderer.gbuffer().depth_buffer.mip_levels())) {
				out_framebuffers.emplace_back(
				        builder.build_framebuffer({renderer.gbuffer().depth_buffer.view(i), util::Rgba{1.f}},
				                                  renderer.gbuffer().depth_buffer.width(i),
				                                  renderer.gbuffer().depth_buffer.height(i)));
			}

			return render_pass;
		}

		auto build_accum_render_pass(Deferred_renderer&                 renderer,
		                             vk::DescriptorSetLayout            desc_set_layout,
		                             vk::Format                         revealage_format,
		                             graphic::Render_target_2D&         accum_buffer,
		                             graphic::Render_target_2D&         revealage_buffer,
		                             std::vector<graphic::Framebuffer>& out_framebuffers)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto depth = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.device().get_depth_format(),
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eLoad,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eDepthStencilAttachmentOptimal,
			                                  vk::ImageLayout::eDepthStencilAttachmentOptimal});

			auto accum = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto revealage = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  revealage_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{
                    vk::PipelineDepthStencilStateCreateFlags{}, true, false, vk::CompareOp::eLess};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);
			pipeline.add_descriptor_set_layout(*renderer.gbuffer().shadowmaps_layout);

			pipeline.add_push_constant("dpc"_strid,
			                           sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto blend_accum     = graphic::Attachment_blend{vk::BlendFactor::eOne,
                                                         vk::BlendFactor::eOne,
                                                         vk::BlendOp::eAdd,
                                                         vk::BlendFactor::eOne,
                                                         vk::BlendFactor::eOne,
                                                         vk::BlendOp::eAdd};
			auto blend_revealage = graphic::Attachment_blend{vk::BlendFactor::eZero,
			                                                 vk::BlendFactor::eOneMinusSrcColor,
			                                                 vk::BlendOp::eAdd,
			                                                 vk::BlendFactor::eZero,
			                                                 vk::BlendFactor::eOneMinusSrcColor,
			                                                 vk::BlendOp::eAdd};

			// TODO: more accum passes for normal/animated models
			auto particle_accum_pipeline = pipeline;
			particle_accum_pipeline.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
			particle_accum_pipeline.add_descriptor_set_layout(renderer.compute_storage_buffer_layout());
			particle_accum_pipeline.vertex<Model_vertex>(0,
			                                             false,
			                                             0,
			                                             &Model_vertex::position,
			                                             1,
			                                             &Model_vertex::normal,
			                                             2,
			                                             &Model_vertex::tex_coords);
			particle_accum_pipeline.vertex<Particle>(
			        1, true, 3, &Particle::position, 4, &Particle::velocity, 5, &Particle::data);

			auto& particle_accum_pass =
			        builder.add_subpass(particle_accum_pipeline)
			                .color_attachment(accum, graphic::all_color_components, blend_accum)
			                .color_attachment(revealage, graphic::all_color_components, blend_revealage)
			                .depth_stencil_attachment(depth, vk::ImageLayout::eDepthStencilReadOnlyOptimal)
			                .input_attachment(depth, vk::ImageLayout::eDepthStencilReadOnlyOptimal);

			auto frag_shadows = renderer.settings().particle_fragment_shadows ? 1 : 0;
			particle_accum_pass.stage("particle_lit"_strid)
			        .shader("frag_shader:particle_transparent_lit"_aid,
			                graphic::Shader_stage::fragment,
			                "main",
			                0,
			                frag_shadows)
			        .shader("vert_shader:particle_transparent_lit"_aid,
			                graphic::Shader_stage::vertex,
			                "main",
			                0,
			                frag_shadows);

			particle_accum_pass.stage("particle"_strid)
			        .shader("frag_shader:particle_transparent_unlit"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:particle_transparent"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			out_framebuffers.reserve(std::size_t(accum_buffer.mip_levels()));

			for(auto i : util::range(accum_buffer.mip_levels())) {
				auto attachments = std::array<graphic::Framebuffer_attachment_desc, 3>{
				        {{renderer.gbuffer().depth_buffer.view(i), util::Rgba{1.f}},
				         {accum_buffer.view(i), util::Rgba{0.f, 0.f, 0.f, 0.f}},
				         {revealage_buffer.view(i), util::Rgba{1.f, 0.f, 0.f, 0.f}}}};

				out_framebuffers.emplace_back(builder.build_framebuffer(
				        attachments, accum_buffer.width(i), accum_buffer.height(i)));
			}

			return render_pass;
		}

		auto build_upsample_render_pass(Deferred_renderer&                 renderer,
		                                vk::DescriptorSetLayout            desc_set_layout,
		                                vk::Format                         revealage_format,
		                                graphic::Render_target_2D&         accum_buffer,
		                                graphic::Render_target_2D&         revealage_buffer,
		                                std::vector<graphic::Framebuffer>& out_framebuffers)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto accum = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto revealage = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  revealage_format,
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

			pipeline.add_push_constant("dpc"_strid,
			                           sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			builder.add_subpass(pipeline)
			        .color_attachment(accum)
			        .color_attachment(revealage)
			        .stage("upsample"_strid)
			        .shader("frag_shader:transparent_upsample"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:fullscreen"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			out_framebuffers.reserve(std::size_t(accum_buffer.mip_levels()));

			for(auto i : util::range(accum_buffer.mip_levels())) {
				auto attachments = std::array<graphic::Framebuffer_attachment_desc, 2>{
				        {{accum_buffer.view(i), util::Rgba{0.f, 0.f, 0.f, 0.f}},
				         {revealage_buffer.view(i), util::Rgba{1.f, 0.f, 0.f, 0.f}}}};

				out_framebuffers.emplace_back(builder.build_framebuffer(
				        attachments, accum_buffer.width(i), accum_buffer.height(i)));
			}

			return render_pass;
		}

		auto build_compose_render_pass(Deferred_renderer&         renderer,
		                               vk::DescriptorSetLayout    desc_set_layout,
		                               graphic::Render_target_2D& target_buffer,
		                               graphic::Framebuffer&      out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto target = builder.add_attachment(
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
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{
                    vk::PipelineDepthStencilStateCreateFlags{}, true, false, vk::CompareOp::eLess};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("dpc"_strid,
			                           sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto blend_compose = graphic::Attachment_blend{vk::BlendFactor::eSrcAlpha,
			                                               vk::BlendFactor::eOneMinusSrcAlpha,
			                                               vk::BlendOp::eAdd,
			                                               vk::BlendFactor::eSrcAlpha,
			                                               vk::BlendFactor::eOneMinusSrcAlpha,
			                                               vk::BlendOp::eAdd};

			auto& compose_pass = builder.add_subpass(pipeline).color_attachment(
			        target, graphic::all_color_components, blend_compose);
			compose_pass.stage("compose"_strid)
			        .shader("frag_shader:transparent_compose"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:transparent_compose"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();
			out_framebuffer =
			        builder.build_framebuffer({target_buffer.view(0), util::Rgba{0.f, 0.f, 0.f, 0.f}},
			                                  target_buffer.width(),
			                                  target_buffer.height());

			return render_pass;
		}

		[[maybe_unused]] void generate_depth_mipmaps(vk::CommandBuffer cb,
		                                             vk::Image         image,
		                                             std::int32_t      width,
		                                             std::int32_t      height,
		                                             std::int32_t      mip_count,
		                                             std::int32_t      start_mip_level)
		{
			using namespace graphic;

			auto aspecs = vk::ImageAspectFlagBits::eDepth;

			if(mip_count == 0) {
				mip_count = static_cast<std::int32_t>(std::floor(std::log2(std::min(width, height))) + 1);
			}

			image_layout_transition(cb,
			                        image,
			                        vk::ImageLayout::eDepthStencilAttachmentOptimal,
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        aspecs,
			                        start_mip_level,
			                        1);

			for(auto level : util::range(1 + start_mip_level, mip_count - 1)) {
				image_layout_transition(cb,
				                        image,
				                        vk::ImageLayout::eUndefined,
				                        vk::ImageLayout::eTransferDstOptimal,
				                        aspecs,
				                        level,
				                        1);

				auto src_range = std::array<vk::Offset3D, 2>{
				        vk::Offset3D{0, 0, 0}, vk::Offset3D{width >> (level - 1), height >> (level - 1), 1}};
				auto dst_range = std::array<vk::Offset3D, 2>{
				        vk::Offset3D{0, 0, 0}, vk::Offset3D{width >> level, height >> level, 1}};

				auto blit = vk::ImageBlit{
				        // src
				        vk::ImageSubresourceLayers{aspecs, gsl::narrow<std::uint32_t>(level - 1), 0, 1},
				        src_range,

				        // dst
				        vk::ImageSubresourceLayers{aspecs, gsl::narrow<std::uint32_t>(level), 0, 1},
				        dst_range};
				cb.blitImage(image,
				             vk::ImageLayout::eTransferSrcOptimal,
				             image,
				             vk::ImageLayout::eTransferDstOptimal,
				             {blit},
				             vk::Filter::eNearest);

				image_layout_transition(cb,
				                        image,
				                        vk::ImageLayout::eTransferDstOptimal,
				                        vk::ImageLayout::eTransferSrcOptimal,
				                        aspecs,
				                        level,
				                        1);
			}

			image_layout_transition(cb,
			                        image,
			                        vk::ImageLayout::eTransferSrcOptimal,
			                        vk::ImageLayout::eDepthStencilAttachmentOptimal,
			                        aspecs,
			                        start_mip_level,
			                        mip_count - start_mip_level);
		}

		auto create_descriptor_set_layout(graphic::Device& device) -> vk::UniqueDescriptorSetLayout
		{
			auto stages   = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex;
			auto bindings = std::array<vk::DescriptorSetLayoutBinding, 6>{
			        vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eStorageBuffer, 1, stages},
			        vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eCombinedImageSampler, 1, stages},
			        vk::DescriptorSetLayoutBinding{2, vk::DescriptorType::eCombinedImageSampler, 1, stages},
			        vk::DescriptorSetLayoutBinding{3, vk::DescriptorType::eCombinedImageSampler, 1, stages},
			        vk::DescriptorSetLayoutBinding{4, vk::DescriptorType::eCombinedImageSampler, 1, stages},
			        vk::DescriptorSetLayoutBinding{
			                5, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment}};

			return device.create_descriptor_set_layout(bindings);
		}

		auto get_revealage_format(graphic::Device& device)
		{
			auto format = device.get_supported_format(
			        {vk::Format::eR8G8B8Unorm, vk::Format::eR8G8B8A8Unorm},
			        vk::FormatFeatureFlagBits::eColorAttachment
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			MIRRAGE_INVARIANT(format.is_some(), "Transparent revealage render targets are not supported!");

			return format.get_or_throw();
		}
	} // namespace


	Transparent_pass::Transparent_pass(Deferred_renderer&         renderer,
	                                   ecs::Entity_manager&       ecs,
	                                   graphic::Render_target_2D& target)
	  : _renderer(renderer)
	  , _ecs(ecs)
	  , _revealage_format(get_revealage_format(renderer.device()))
	  , _accum(renderer.device(),
	           {renderer.gbuffer().depth.width(), renderer.gbuffer().depth.height()},
	           renderer.gbuffer().depth.mip_levels(),
	           renderer.gbuffer().color_format,
	           vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	           vk::ImageAspectFlagBits::eColor)
	  , _revealage(renderer.device(),
	               {renderer.gbuffer().depth.width(), renderer.gbuffer().depth.height()},
	               renderer.gbuffer().depth.mip_levels(),
	               _revealage_format,
	               vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	               vk::ImageAspectFlagBits::eColor)

	  , _sampler(renderer.device().create_sampler(1,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eLinear,
	                                              vk::SamplerMipmapMode::eNearest))
	  , _desc_set_layout(create_descriptor_set_layout(renderer.device()))

	  , _depth_sampler(renderer.device().create_sampler(1,
	                                                    vk::SamplerAddressMode::eClampToEdge,
	                                                    vk::BorderColor::eIntOpaqueBlack,
	                                                    vk::Filter::eNearest,
	                                                    vk::SamplerMipmapMode::eNearest))
	  , _mip_desc_set_layout(renderer.device(), *_depth_sampler, 1)
	  , _mip_render_pass(build_mip_render_pass(renderer, *_mip_desc_set_layout, _mip_framebuffers))

	  , _accum_render_pass(build_accum_render_pass(
	            renderer, *_desc_set_layout, _revealage_format, _accum, _revealage, _accum_framebuffers))
	  , _upsample_render_pass(build_upsample_render_pass(
	            renderer, *_desc_set_layout, _revealage_format, _accum, _revealage, _upsample_framebuffers))
	  , _compose_render_pass(
	            build_compose_render_pass(renderer, *_desc_set_layout, target, _compose_framebuffer))
	  , _light_uniforms(renderer.device().transfer().create_dynamic_buffer(
	            std::int32_t(sizeof(Directional_light_uniforms) * 4 + 4 * 4),
	            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
	            vk::PipelineStageFlagBits::eVertexShader,
	            vk::AccessFlagBits::eShaderRead,
	            vk::PipelineStageFlagBits::eFragmentShader,
	            vk::AccessFlagBits::eShaderRead))
	  , _light_uniforms_tmp(sizeof(Directional_light_uniforms) * 4 + 4 * 4)
	{
		auto light_data_info = vk::DescriptorBufferInfo(_light_uniforms.buffer(), 0, VK_WHOLE_SIZE);

		_accum_descriptor_sets = util::build_vector(renderer.gbuffer().depth.mip_levels(), [&](auto i) {
			auto set = renderer.create_descriptor_set(*_desc_set_layout, 3);

			auto depth_info = vk::DescriptorImageInfo({},
			                                          renderer.gbuffer().depth_buffer.view(i),
			                                          vk::ImageLayout::eDepthStencilReadOnlyOptimal);

			auto desc_writes = std::array<vk::WriteDescriptorSet, 2>{
			        vk::WriteDescriptorSet{
			                *set, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &light_data_info},
			        vk::WriteDescriptorSet{*set, 5, 0, 1, vk::DescriptorType::eInputAttachment, &depth_info}};


			renderer.device().vk_device()->updateDescriptorSets(
			        gsl::narrow<std::uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);

			return set;
		});

		_upsample_descriptor_sets = util::build_vector(renderer.gbuffer().depth.mip_levels(), [&](auto i) {
			auto set        = renderer.create_descriptor_set(*_desc_set_layout, 5);
			auto accum_info = vk::DescriptorImageInfo(
			        *_sampler, _accum.view(i), vk::ImageLayout::eShaderReadOnlyOptimal);

			auto revealage = vk::DescriptorImageInfo(
			        *_sampler, _revealage.view(i), vk::ImageLayout::eShaderReadOnlyOptimal);

			auto depth_base   = vk::DescriptorImageInfo(*_sampler,
                                                      renderer.gbuffer().depth_buffer.view(i),
                                                      vk::ImageLayout::eShaderReadOnlyOptimal);
			auto depth_target = vk::DescriptorImageInfo(*_sampler,
			                                            renderer.gbuffer().depth_buffer.view(0),
			                                            vk::ImageLayout::eShaderReadOnlyOptimal);

			auto desc_writes = std::array<vk::WriteDescriptorSet, 5>{
			        vk::WriteDescriptorSet{
			                *set, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &light_data_info},
			        vk::WriteDescriptorSet{
			                *set, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &accum_info},
			        vk::WriteDescriptorSet{
			                *set, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &revealage},
			        vk::WriteDescriptorSet{
			                *set, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &depth_base},
			        vk::WriteDescriptorSet{
			                *set, 4, 0, 1, vk::DescriptorType::eCombinedImageSampler, &depth_target}};


			renderer.device().vk_device()->updateDescriptorSets(
			        gsl::narrow<std::uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);

			return set;
		});

		if(_renderer.gbuffer().depth_sampleable) {
			LOG(plog::info) << "Using shader based mip generation for depth";

			_mip_descriptor_sets = util::build_vector(renderer.gbuffer().depth.mip_levels(), [&](auto i) {
				auto set  = renderer.create_descriptor_set(*_mip_desc_set_layout, 1);
				auto info = vk::DescriptorImageInfo(*_depth_sampler,
				                                    renderer.gbuffer().depth_buffer.view(i),
				                                    vk::ImageLayout::eShaderReadOnlyOptimal);

				auto desc_writes = std::array<vk::WriteDescriptorSet, 1>{vk::WriteDescriptorSet{
				        *set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &info}};


				renderer.device().vk_device()->updateDescriptorSets(
				        gsl::narrow<std::uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);

				return set;
			});

		} else {
			LOG(plog::info) << "Using blit based mip generation for depth";
		}


		_compose_descriptor_sets = util::build_vector(renderer.gbuffer().depth.mip_levels(), [&](auto i) {
			auto set = renderer.create_descriptor_set(*_desc_set_layout, 3);

			auto accum_info = vk::DescriptorImageInfo(
			        *_sampler, _accum.view(i), vk::ImageLayout::eShaderReadOnlyOptimal);

			auto revealage = vk::DescriptorImageInfo(
			        *_sampler, _revealage.view(i), vk::ImageLayout::eShaderReadOnlyOptimal);

			auto desc_writes = std::array<vk::WriteDescriptorSet, 3>{
			        vk::WriteDescriptorSet{
			                *set, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &light_data_info},
			        vk::WriteDescriptorSet{
			                *set, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &accum_info},
			        vk::WriteDescriptorSet{
			                *set, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &revealage}};


			renderer.device().vk_device()->updateDescriptorSets(
			        gsl::narrow<std::uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);

			return set;
		});
	} // namespace mirrage::renderer


	void Transparent_pass::update(util::Time) {}

	void Transparent_pass::draw(Frame_data& frame)
	{
		Push_constants dpc{};

		// TODO: contains duplicated code from deferred_geometry_subpass. Should be refactor-able after the culling-rewrite
		if(!_renderer.billboard_model().ready())
			return;

		// update _light_uniforms
		auto light_uniforms_ptr = _light_uniforms_tmp.data();
		auto lights_out         = gsl::span<Directional_light_uniforms>(
                reinterpret_cast<Directional_light_uniforms*>(light_uniforms_ptr + 4 * 4), 4);

		auto ambient_factor = util::max(0.1f, _renderer.settings().amient_light_intensity);
		auto light_count    = 0;
		for(auto [transform, light] : _ecs.list<ecs::components::Transform_comp, Directional_light_comp>()) {
			if(light.light_particles()) {
				lights_out[light_count].light_space =
				        light.calc_shadowmap_view_proj(transform) * _renderer.global_uniforms().inv_view_mat;
				lights_out[light_count].radiance =
				        glm::vec4(light.color() * light.intensity() / 10000.0f, 1.f);
				lights_out[light_count].shadow_radiance = glm::vec4(
				        light.shadow_color() * light.shadow_intensity() / 10000.0f * ambient_factor, 1.f);

				auto dir = _renderer.global_uniforms().view_mat * glm::vec4(-transform.direction(), 0.f);
				dir      = glm::normalize(dir);
				dir.w    = gsl::narrow<float>(_renderer.gbuffer().shadowmaps ? light.shadowmap_id() : -1);
				lights_out[light_count].dir = dir;

				light_count++;
			}
		}
		*reinterpret_cast<std::int32_t*>(light_uniforms_ptr) = light_count;

		_light_uniforms.update(frame.main_command_buffer, 0, _light_uniforms_tmp);


		// TODO: draw normal/anim models

		auto particle_begin =
		        std::find_if(frame.particle_queue.begin(), frame.particle_queue.end(), [&](auto& p) {
			        return (p.type_cfg->blend == Particle_blend_mode::transparent
			                || p.type_cfg->blend == Particle_blend_mode::transparent_unlit)
			               && (p.culling_mask & 1) != 0 && p.emitter->drawable();
		        });

		if(particle_begin == frame.particle_queue.end())
			return;

		auto mip_level = _renderer.settings().transparent_particle_mip_level;

		// generate depth mipmaps
		if(_renderer.gbuffer().depth_sampleable) {
			for(auto dst = 1; dst <= mip_level; dst++) {
				graphic::image_layout_transition(frame.main_command_buffer,
				                                 _renderer.gbuffer().depth_buffer.image(),
				                                 vk::ImageLayout::eDepthStencilAttachmentOptimal,
				                                 vk::ImageLayout::eShaderReadOnlyOptimal,
				                                 vk::ImageAspectFlagBits::eDepth,
				                                 dst - 1,
				                                 1);

				auto& fb = _mip_framebuffers.at(gsl::narrow<std::size_t>(dst));
				_mip_render_pass.execute(frame.main_command_buffer, fb, [&] {
					_mip_render_pass.bind_descriptor_set(1, *_mip_descriptor_sets.at(dst - 1));

					frame.main_command_buffer.draw(3, 1, 0, 0);
				});

				graphic::image_layout_transition(frame.main_command_buffer,
				                                 _renderer.gbuffer().depth_buffer.image(),
				                                 vk::ImageLayout::eShaderReadOnlyOptimal,
				                                 vk::ImageLayout::eDepthStencilAttachmentOptimal,
				                                 vk::ImageAspectFlagBits::eDepth,
				                                 dst - 1,
				                                 1);
			}

		} else {
			generate_depth_mipmaps(frame.main_command_buffer,
			                       _renderer.gbuffer().depth_buffer.image(),
			                       _renderer.gbuffer().depth_buffer.width(),
			                       _renderer.gbuffer().depth_buffer.height(),
			                       mip_level + 1,
			                       0);
		}

		auto& fb = _accum_framebuffers.at(gsl::narrow<std::size_t>(mip_level));
		_accum_render_pass.execute(frame.main_command_buffer, fb, [&] {
			auto last_material = static_cast<const Material*>(nullptr);
			auto last_model    = static_cast<const Model*>(nullptr);

			auto desc_sets = std::array<vk::DescriptorSet, 3>{frame.global_uniform_set,
			                                                  *_accum_descriptor_sets.at(mip_level),
			                                                  *_renderer.gbuffer().shadowmaps};
			_accum_render_pass.bind_descriptor_sets(0, desc_sets);

			// draw particles
			for(auto& particle : util::range(particle_begin, frame.particle_queue.end())) {
				if((particle.culling_mask & 1) == 0 || !particle.emitter->drawable())
					break;

				if(particle.type_cfg->blend == Particle_blend_mode::transparent_unlit) {
					_accum_render_pass.set_stage("particle"_strid);
				} else if(particle.type_cfg->blend == Particle_blend_mode::transparent) {
					_accum_render_pass.set_stage("particle_lit"_strid);
				} else
					break;

				auto material = &*particle.type_cfg->material;
				if(material != last_material) {
					last_material = material;
					last_material->bind(_accum_render_pass, 3);
				}

				// bind emitter data
				_accum_render_pass.bind_descriptor_set(4, particle.emitter->particle_uniforms());

				// bind model
				auto model =
				        particle.type_cfg->model ? &*particle.type_cfg->model : &_renderer.billboard_model();
				if(model != last_model) {
					last_model = model;
					last_model->bind_mesh(frame.main_command_buffer, 0);
				}

				auto emissive_color = glm::vec4(1, 1, 1, 1000);
				if(auto e = _ecs.get(particle.entity); e.is_some()) {
					e.get_or_throw().process<Material_property_comp>(
					        [&](auto& m) { emissive_color = m.emissive_color; });
				}
				emissive_color.a /= 10000.0f;
				if(last_material && !last_material->has_emission()) {
					emissive_color.a = 0;
				}
				dpc.light_data = emissive_color;

				// bind particle data
				frame.main_command_buffer.bindVertexBuffers(
				        1,
				        {particle.emitter->particle_buffer()},
				        {vk::DeviceSize(particle.emitter->particle_offset())
				         * vk::DeviceSize(sizeof(Particle))});

				dpc.light_data2.w = static_cast<float>(mip_level);
				dpc.model         = glm::mat4(1);
				if(particle.type_cfg->geometry == Particle_geometry::billboard) {
					dpc.model    = glm::inverse(_renderer.global_uniforms().view_mat);
					dpc.model[3] = glm::vec4(0, 0, 0, 1);
				}
				_accum_render_pass.push_constant("dpc"_strid, dpc);

				// draw instanced
				auto& sub_mesh = last_model->sub_meshes().at(0);
				frame.main_command_buffer.drawIndexed(sub_mesh.index_count,
				                                      std::uint32_t(particle.emitter->particle_count()),
				                                      sub_mesh.index_offset,
				                                      0,
				                                      0);
			}
		});


		if(mip_level > _renderer.settings().upsample_transparency_to_mip) {
			auto target_mip = _renderer.settings().upsample_transparency_to_mip;
			// upsample low-res buffers
			graphic::image_layout_transition(frame.main_command_buffer,
			                                 _renderer.gbuffer().depth_buffer.image(),
			                                 vk::ImageLayout::eDepthStencilAttachmentOptimal,
			                                 vk::ImageLayout::eShaderReadOnlyOptimal,
			                                 vk::ImageAspectFlagBits::eDepth,
			                                 0,
			                                 mip_level + 1);

			for(auto i = mip_level - 1; i >= target_mip; i--) {
				_upsample_render_pass.execute(
				        frame.main_command_buffer,
				        _upsample_framebuffers.at(gsl::narrow<std::size_t>(i)),
				        [&] {
					        _upsample_render_pass.bind_descriptor_set(
					                1, *_upsample_descriptor_sets.at(gsl::narrow<std::size_t>(i + 1)));
					        frame.main_command_buffer.draw(3, 1, 0, 0);
				        });
			}

			graphic::image_layout_transition(frame.main_command_buffer,
			                                 _renderer.gbuffer().depth_buffer.image(),
			                                 vk::ImageLayout::eShaderReadOnlyOptimal,
			                                 vk::ImageLayout::eDepthStencilAttachmentOptimal,
			                                 vk::ImageAspectFlagBits::eDepth,
			                                 0,
			                                 mip_level + 1);

			mip_level = target_mip;
		}

		// compose into frame
		_compose_render_pass.execute(frame.main_command_buffer, _compose_framebuffer, [&] {
			_compose_render_pass.bind_descriptor_set(1, *_compose_descriptor_sets.at(mip_level));
			frame.main_command_buffer.draw(3, 1, 0, 0);
		});
	}


	auto Transparent_pass_factory::create_pass(Deferred_renderer& renderer,
	                                           std::shared_ptr<void>,
	                                           util::maybe<ecs::Entity_manager&> ecs,
	                                           Engine&,
	                                           bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		if(!renderer.device().physical_device_features().independentBlend || ecs.is_nothing())
			return {};

		auto& target = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;
		return std::make_unique<Transparent_pass>(renderer, ecs.get_or_throw(), target);
	}

	auto Transparent_pass_factory::rank_device(vk::PhysicalDevice,
	                                           util::maybe<std::uint32_t>,
	                                           int current_score) -> int
	{
		return current_score;
	}

	void Transparent_pass_factory::configure_device(vk::PhysicalDevice pd,
	                                                util::maybe<std::uint32_t>,
	                                                graphic::Device_create_info& ci)
	{
		auto features = pd.getFeatures();
		if(features.independentBlend) {
			ci.features.independentBlend = true;
		} else {
			LOG(plog::warning) << "Feature independentBlend is not supported. Transparent objects will not "
			                      "be rendered!";
		}

		if(features.fragmentStoresAndAtomics) {
			ci.features.fragmentStoresAndAtomics = true;
		} else {
			LOG(plog::warning) << "Feature fragmentStoresAndAtomics is not supported.";
		}

		if(features.vertexPipelineStoresAndAtomics) {
			ci.features.vertexPipelineStoresAndAtomics = true;
		} else {
			LOG(plog::warning) << "Feature vertexPipelineStoresAndAtomics is not supported.";
		}
	}
} // namespace mirrage::renderer
