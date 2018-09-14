#include <mirrage/renderer/pass/gi_pass.hpp>

#include <mirrage/graphic/window.hpp>

#include <glm/gtx/norm.hpp>

#include <numeric>
#include <unordered_set>

namespace mirrage::renderer {

	using namespace graphic;


	namespace {
		struct Gi_constants {
			glm::mat4 reprojection;    // prev_view * inverse(view)
			glm::mat4 prev_projection; // m[3].xy = current level, max level
		};


		auto build_integrate_brdf_render_pass(Deferred_renderer& renderer,
		                                      vk::Format         target_format,
		                                      Render_target_2D&  target,
		                                      Framebuffer&       out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  target_format,
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

			auto& pass = builder.add_subpass(pipeline).color_attachment(color);

			pass.stage("integrate"_strid)
			        .shader("frag_shader:gi_integrate_brdf"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_integrate_brdf"_aid, graphic::Shader_stage::vertex);

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

			out_framebuffer =
			        builder.build_framebuffer({target.view(), util::Rgba{}}, target.width(), target.height());

			return render_pass;
		}

		auto build_reproject_pass(Deferred_renderer&      renderer,
		                          vk::DescriptorSetLayout desc_set_layout,
		                          vk::Format              history_weight_format,
		                          int                     min_mip_level,
		                          Render_target_2D&       input,
		                          Render_target_2D&       diffuse,
		                          Render_target_2D&       specular,
		                          Render_target_2D&       history_success,
		                          Framebuffer&            out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color_input = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eLoad,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});
			auto color_diffuse = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});
			auto color_specular = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});
			auto color_success = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  history_weight_format,
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
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline)
			                     .color_attachment(color_input,
			                                       graphic::all_color_components,
			                                       graphic::blend_premultiplied_alpha)
			                     .color_attachment(color_diffuse,
			                                       graphic::all_color_components,
			                                       graphic::blend_premultiplied_alpha)
			                     .color_attachment(color_specular,
			                                       graphic::all_color_components,
			                                       graphic::blend_premultiplied_alpha)
			                     .color_attachment(color_success,
			                                       graphic::all_color_components,
			                                       graphic::blend_premultiplied_alpha);

			pass.stage("reproject"_strid)
			        .shader("frag_shader:gi_reproject"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_reproject"_aid, graphic::Shader_stage::vertex);

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

			auto attachments = std::array<Framebuffer_attachment_desc, 4>{
			        {{input.view(min_mip_level), util::Rgba{}},
			         {diffuse.view(0), util::Rgba{}},
			         {specular.view(0), util::Rgba{0, 0, 0, 0}},
			         {history_success.view(0), util::Rgba{0, 0, 0, 0}}}};

			out_framebuffer = builder.build_framebuffer(attachments, diffuse.width(0), diffuse.height(0));

			return render_pass;
		}

		auto build_reproject_weight_pass(Deferred_renderer&      renderer,
		                                 vk::DescriptorSetLayout desc_set_layout,
		                                 vk::Format              history_weight_format,
		                                 Render_target_2D&       history_weight,
		                                 Framebuffer&            out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color_weight = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  history_weight_format,
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
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        color_weight, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			pass.stage("reproject"_strid)
			        .shader("frag_shader:gi_reprojection_weights"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_reprojection_weights"_aid, graphic::Shader_stage::vertex);

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

			auto attachment = Framebuffer_attachment_desc{history_weight.view(0), util::Rgba{}};

			out_framebuffer =
			        builder.build_framebuffer(attachment, history_weight.width(0), history_weight.height(0));

			return render_pass;
		}

		auto build_weight_mipgen_pass(Deferred_renderer&        renderer,
		                              vk::DescriptorSetLayout   desc_set_layout,
		                              vk::Format                history_weight_format,
		                              Render_target_2D&         history_weight,
		                              std::vector<Framebuffer>& out_framebuffers)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color_weight = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  history_weight_format,
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
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(color_weight);

			pass.stage("reproject"_strid)
			        .shader("frag_shader:gi_weight_mipgen"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_weight_mipgen"_aid, graphic::Shader_stage::vertex);

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

			out_framebuffers = util::build_vector(history_weight.mip_levels(), [&](auto i) {
				return builder.build_framebuffer({history_weight.view(i), util::Rgba{}},
				                                 history_weight.width(i),
				                                 history_weight.height(i));
			});

			return render_pass;
		}

		auto build_diffuse_reproject_pass(Deferred_renderer&        renderer,
		                                  vk::DescriptorSetLayout   desc_set_layout,
		                                  Render_target_2D&         diffuse,
		                                  std::vector<Framebuffer>& out_framebuffers)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color_diffuse = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eDontCare,
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
			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        color_diffuse, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			pass.stage("reproject"_strid)
			        .shader("frag_shader:gi_diffuse_reproject"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_diffuse_reproject"_aid, graphic::Shader_stage::vertex);

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

			for(auto j : util::range(diffuse.mip_levels())) {
				out_framebuffers.emplace_back(
				        builder.build_framebuffer(Framebuffer_attachment_desc{diffuse.view(j), util::Rgba{}},
				                                  gsl::narrow<int>(diffuse.width(j)),
				                                  gsl::narrow<int>(diffuse.height(j))));
			}

			return render_pass;
		}

		auto build_sample_render_pass(Deferred_renderer&        renderer,
		                              vk::DescriptorSetLayout   desc_set_layout,
		                              int                       min_mip_level,
		                              int                       max_mip_level,
		                              int                       sample_count,
		                              int                       first_level_sample_count,
		                              Render_target_2D&         gi_buffer_samples,
		                              Render_target_2D&         gi_buffer_result,
		                              std::vector<Framebuffer>& out_framebuffers_samples,
		                              std::vector<Framebuffer>& out_framebuffers_result)
		{

			auto builder = renderer.device().create_render_pass_builder();

			// TODO: loadOp could be Don't-Care for all passes, except upsample!
			auto color = builder.add_attachment(
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
			pipeline.add_descriptor_set_layout(renderer.noise_descriptor_set_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        color, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			// diffuse sample
			pass.stage("sample_first"_strid)
			        .shader("frag_shader:gi_sample"_aid,
			                graphic::Shader_stage::fragment,
			                "main",
			                2,
			                first_level_sample_count)
			        .shader("vert_shader:gi_sample"_aid, graphic::Shader_stage::vertex);

			pass.stage("sample"_strid)
			        .shader("frag_shader:gi_sample"_aid,
			                graphic::Shader_stage::fragment,
			                "main",
			                2,
			                sample_count)
			        .shader("vert_shader:gi_sample"_aid, graphic::Shader_stage::vertex);

			// circle instead of ring sample-pattern
			pass.stage("sample_last"_strid)
			        .shader("frag_shader:gi_sample"_aid,
			                graphic::Shader_stage::fragment,
			                "main",
			                0,
			                1,
			                2,
			                sample_count)
			        .shader("vert_shader:gi_sample"_aid, graphic::Shader_stage::vertex);

			// upsample the previous level
			pass.stage("upsample"_strid)
			        .shader("frag_shader:gi_sample_upsample"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_sample_upsample"_aid, graphic::Shader_stage::vertex);
			pass.stage("upsample_np"_strid)
			        .shader("frag_shader:gi_sample_upsample"_aid,
			                graphic::Shader_stage::fragment,
			                "main",
			                0,
			                1)
			        .shader("vert_shader:gi_sample_upsample"_aid, graphic::Shader_stage::vertex);

			// upsample the previous level
			pass.stage("blend"_strid)
			        .shader("frag_shader:gi_sample_blend"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_sample_blend"_aid, graphic::Shader_stage::vertex);

			// upsample the previous level
			pass.stage("blend_last"_strid)
			        .shader("frag_shader:gi_sample_blend"_aid, graphic::Shader_stage::fragment, "main", 0, 1)
			        .shader("vert_shader:gi_sample_blend"_aid, graphic::Shader_stage::vertex);


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

			auto end = max_mip_level;
			for(auto j = 0; j < end - min_mip_level; j++) {
				out_framebuffers_samples.emplace_back(
				        builder.build_framebuffer({gi_buffer_samples.view(j), util::Rgba{}},
				                                  gi_buffer_samples.width(j),
				                                  gi_buffer_samples.height(j)));

				out_framebuffers_result.emplace_back(
				        builder.build_framebuffer({gi_buffer_result.view(j), util::Rgba{}},
				                                  gi_buffer_result.width(j),
				                                  gi_buffer_result.height(j)));
			}

			return render_pass;
		}

		auto build_sample_spec_render_pass(Deferred_renderer&      renderer,
		                                   vk::DescriptorSetLayout desc_set_layout,
		                                   Render_target_2D&       gi_spec_buffer,
		                                   Framebuffer&            out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color = builder.add_attachment(
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
			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        color, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			pass.stage("sample"_strid)
			        .shader("frag_shader:gi_sample_spec"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_sample_spec"_aid, graphic::Shader_stage::vertex);

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

			out_framebuffer = builder.build_framebuffer({gi_spec_buffer.view(0), util::Rgba{}},
			                                            gsl::narrow<int>(gi_spec_buffer.width()),
			                                            gsl::narrow<int>(gi_spec_buffer.height()));

			return render_pass;
		}

		auto build_median_spec_render_pass(Deferred_renderer&      renderer,
		                                   vk::DescriptorSetLayout desc_set_layout,
		                                   Render_target_2D&       gi_spec_buffer,
		                                   Framebuffer&            out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color = builder.add_attachment(
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
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        color, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			pass.stage("median"_strid)
			        .shader("frag_shader:median_filter"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:median_filter"_aid, graphic::Shader_stage::vertex);

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

			out_framebuffer = builder.build_framebuffer({gi_spec_buffer.view(0), util::Rgba{}},
			                                            gsl::narrow<int>(gi_spec_buffer.width()),
			                                            gsl::narrow<int>(gi_spec_buffer.height()));

			return render_pass;
		}

		auto build_blur_render_pass(Deferred_renderer&         renderer,
		                            vk::DescriptorSetLayout    desc_set_layout,
		                            graphic::Render_target_2D& blur_buffer,
		                            graphic::Render_target_2D& result_buffer,
		                            graphic::Framebuffer&      out_blur_framebuffer,
		                            graphic::Framebuffer&      out_result_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color = builder.add_attachment(
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
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(color);

			pass.stage("blur_h"_strid)
			        .shader("frag_shader:gi_spec_blur"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_spec_blur"_aid, graphic::Shader_stage::vertex, "main", 0, 1);

			pass.stage("blur_v"_strid)
			        .shader("frag_shader:gi_spec_blur"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_spec_blur"_aid, graphic::Shader_stage::vertex, "main", 0, 0);

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

			out_blur_framebuffer = builder.build_framebuffer(
			        {blur_buffer.view(0), util::Rgba{}}, blur_buffer.width(), blur_buffer.height());

			out_result_framebuffer = builder.build_framebuffer(
			        {result_buffer.view(0), util::Rgba{}}, result_buffer.width(), result_buffer.height());

			return render_pass;
		}

		auto build_blend_render_pass(Deferred_renderer&      renderer,
		                             vk::DescriptorSetLayout desc_set_layout,
		                             Render_target_2D&       color_in_out,
		                             Framebuffer&            out_framebuffer)
		{

			auto builder = renderer.device().create_render_pass_builder();

			auto color = builder.add_attachment(
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
			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        color, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			pass.stage("mipgen"_strid)
			        .shader("frag_shader:gi_blend"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:gi_blend"_aid, graphic::Shader_stage::vertex);

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

			out_framebuffer = builder.build_framebuffer({color_in_out.view(0), util::Rgba{}},
			                                            gsl::narrow<int>(color_in_out.width()),
			                                            gsl::narrow<int>(color_in_out.height()));

			return render_pass;
		}

		auto get_brdf_format(graphic::Device& device)
		{
			auto format = device.get_supported_format(
			        {vk::Format::eR16G16Sfloat},
			        vk::FormatFeatureFlagBits::eColorAttachment
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear
			                | vk::FormatFeatureFlagBits::eBlitDst | vk::FormatFeatureFlagBits::eBlitSrc);

			MIRRAGE_INVARIANT(format.is_some(),
			                  "No Float R16G16 format supported (required for BRDF preintegration)!");

			return format.get_or_throw();
		}
		auto get_history_weight_format(graphic::Device& device)
		{
			auto format = device.get_supported_format(
			        {vk::Format::eR16Sfloat},
			        vk::FormatFeatureFlagBits::eColorAttachment
			                | vk::FormatFeatureFlagBits::eColorAttachmentBlend
			                | vk::FormatFeatureFlagBits::eBlitDst
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			MIRRAGE_INVARIANT(format.is_some(), "No Float R16 format supported!");

			return format.get_or_throw();
		}
		auto get_history_success_format(graphic::Device& device)
		{
			auto format = device.get_supported_format(
			        {vk::Format::eR16G16Sfloat},
			        vk::FormatFeatureFlagBits::eColorAttachment
			                | vk::FormatFeatureFlagBits::eColorAttachmentBlend
			                | vk::FormatFeatureFlagBits::eBlitDst
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			MIRRAGE_INVARIANT(format.is_some(), "No Float R16 format supported!");

			return format.get_or_throw();
		}

		auto calc_base_mip_level(std::int32_t width, std::int32_t height, bool highres)
		{
			auto x_mip = glm::log2(float(width) / 960.f);
			auto y_mip = glm::log2(float(height) / 500.f);

			auto mip = highres ? util::min(x_mip, y_mip) : util::max(x_mip, y_mip);

			return static_cast<std::int32_t>(util::max(0, static_cast<int>(std::round(mip))));
		}
		auto calc_max_mip_level(std::int32_t width, std::int32_t height)
		{
			auto w        = static_cast<float>(width);
			auto h        = static_cast<float>(height);
			auto diagonal = std::sqrt(w * w + h * h);

			return static_cast<std::int32_t>(std::ceil(glm::log2(diagonal / 40.f)));
		}

		template <class T>
		auto to_2prod(T v)
		{
			return v % 2 == 0 ? v : v + T(1);
		}
	} // namespace


	Gi_pass::Gi_pass(Deferred_renderer&         renderer,
	                 graphic::Render_target_2D& in_out,
	                 graphic::Render_target_2D& diffuse_in)
	  : _renderer(renderer)
	  , _highres_base_mip_level(calc_base_mip_level(in_out.width(), in_out.height(), true))
	  , _base_mip_level(calc_base_mip_level(in_out.width(), in_out.height(), renderer.settings().gi_highres))
	  , _max_mip_level(calc_max_mip_level(in_out.width(), in_out.height()))
	  , _diffuse_mip_level(_base_mip_level + renderer.settings().gi_diffuse_mip_level)
	  , _min_mip_level(std::min(_diffuse_mip_level, _base_mip_level + renderer.settings().gi_min_mip_level))
	  , _gbuffer_sampler(renderer.device().create_sampler(12,
	                                                      vk::SamplerAddressMode::eClampToEdge,
	                                                      vk::BorderColor::eIntOpaqueBlack,
	                                                      vk::Filter::eLinear,
	                                                      vk::SamplerMipmapMode::eLinear))
	  , _descriptor_set_layout(renderer.device(),
	                           *_gbuffer_sampler,
	                           11,
	                           vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex)
	  , _color_in_out(in_out)
	  , _color_diffuse_in(diffuse_in)

	  , _gi_diffuse_history(graphic::Render_target_2D{
	            renderer.device(),
	            {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	            0,
	            renderer.gbuffer().color_format,
	            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
	                    | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment,
	            vk::ImageAspectFlagBits::eColor})
	  , _gi_diffuse_samples(graphic::Render_target_2D{
	            renderer.device(),
	            {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	            0,
	            renderer.gbuffer().color_format,
	            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
	                    | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment,
	            vk::ImageAspectFlagBits::eColor})
	  , _gi_diffuse_result(graphic::Render_target_2D{
	            renderer.device(),
	            {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	            0,
	            renderer.gbuffer().color_format,
	            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
	                    | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment,
	            vk::ImageAspectFlagBits::eColor})

	  , _gi_specular(renderer.device(),
	                 {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	                 1,
	                 renderer.gbuffer().color_format,
	                 vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc
	                         | vk::ImageUsageFlagBits::eTransferDst
	                         | vk::ImageUsageFlagBits::eColorAttachment,
	                 vk::ImageAspectFlagBits::eColor)

	  , _gi_specular_history(renderer.device(),
	                         {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	                         1,
	                         renderer.gbuffer().color_format,
	                         vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst
	                                 | vk::ImageUsageFlagBits::eColorAttachment,
	                         vk::ImageAspectFlagBits::eColor)

	  , _history_success_format(get_history_success_format(renderer.device()))
	  , _history_weight_format(get_history_weight_format(renderer.device()))
	  , _history_success(renderer.device(),
	                     {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	                     1,
	                     _history_success_format,
	                     vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                             | vk::ImageUsageFlagBits::eTransferDst
	                             | vk::ImageUsageFlagBits::eTransferSrc,
	                     vk::ImageAspectFlagBits::eColor)
	  , _history_weight(renderer.device(),
	                    {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	                    0,
	                    _history_weight_format,
	                    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                            | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
	                    vk::ImageAspectFlagBits::eColor)

	  , _integrated_brdf_format(get_brdf_format(renderer.device()))
	  , _integrated_brdf(renderer.device(),
	                     {512, 512},
	                     1,
	                     _integrated_brdf_format,
	                     vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
	                     vk::ImageAspectFlagBits::eColor)
	  , _brdf_integration_renderpass(build_integrate_brdf_render_pass(
	            renderer, _integrated_brdf_format, _integrated_brdf, _brdf_integration_framebuffer))

	  , _reproject_renderpass(build_reproject_pass(renderer,
	                                               *_descriptor_set_layout,
	                                               _history_success_format,
	                                               _min_mip_level,
	                                               _color_diffuse_in,
	                                               _gi_diffuse_history,
	                                               _gi_specular,
	                                               _history_success,
	                                               _reproject_framebuffer))
	  , _reproject_descriptor_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                                                {renderer.gbuffer().depth.view(),
	                                                                 renderer.gbuffer().mat_data.view(),
	                                                                 renderer.gbuffer().albedo_mat_id.view(0),
	                                                                 _gi_diffuse_result.view(),
	                                                                 _gi_specular_history.view(),
	                                                                 renderer.gbuffer().prev_depth.view(),
	                                                                 _integrated_brdf.view(),
	                                                                 _history_weight.view(0)}))

	  , _reproject_weight_renderpass(build_reproject_weight_pass(renderer,
	                                                             *_descriptor_set_layout,
	                                                             _history_weight_format,
	                                                             _history_weight,
	                                                             _reproject_weight_framebuffer))
	  , _reproject_weight_descriptor_set(
	            _descriptor_set_layout.create_set(renderer.descriptor_pool(), {_history_success.view()}))

	  , _weight_mipgen_renderpass(build_weight_mipgen_pass(renderer,
	                                                       *_descriptor_set_layout,
	                                                       _history_weight_format,
	                                                       _history_weight,
	                                                       _weight_mipgen_framebuffers))
	  , _weight_mipgen_descriptor_sets(util::build_vector(_history_weight.mip_levels(),
	                                                      [&](auto i) {
		                                                      return _descriptor_set_layout.create_set(
		                                                              renderer.descriptor_pool(),
		                                                              {_history_weight.view(i)});
	                                                      }))


	  , _diffuse_reproject_renderpass(build_diffuse_reproject_pass(
	            renderer, *_descriptor_set_layout, _gi_diffuse_history, _diffuse_reproject_framebuffers))

	  , _sample_renderpass(build_sample_render_pass(renderer,
	                                                *_descriptor_set_layout,
	                                                _min_mip_level,
	                                                _max_mip_level,
	                                                to_2prod(renderer.settings().gi_samples),
	                                                to_2prod(renderer.settings().gi_lowres_samples),
	                                                _gi_diffuse_samples,
	                                                _gi_diffuse_result,
	                                                _sample_framebuffers,
	                                                _sample_framebuffers_blend))

	  , _sample_spec_renderpass(build_sample_spec_render_pass(
	            renderer, *_descriptor_set_layout, _gi_specular, _sample_spec_framebuffer))
	  , _sample_spec_descriptor_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                                                  {_color_diffuse_in.view(),
	                                                                   renderer.gbuffer().depth.view(),
	                                                                   renderer.gbuffer().mat_data.view(),
	                                                                   _history_weight.view(),
	                                                                   _gi_diffuse_history.view(0)}))

	  , _median_spec_renderpass(build_median_spec_render_pass(
	            renderer, *_descriptor_set_layout, _gi_specular_history, _median_spec_framebuffer))
	  , _median_spec_descriptor_set(
	            _descriptor_set_layout.create_set(renderer.descriptor_pool(), {_gi_specular.view(0)}))

	  , _blur_render_pass(build_blur_render_pass(renderer,
	                                             *_descriptor_set_layout,
	                                             _gi_specular_history,
	                                             _gi_specular,
	                                             _blur_vertical_framebuffer,
	                                             _blur_horizonal_framebuffer))
	  , _blur_descriptor_set_horizontal(_descriptor_set_layout.create_set(
	            renderer.descriptor_pool(), {renderer.gbuffer().depth.view(), _gi_specular_history.view()}))
	  , _blur_descriptor_set_vertical(_descriptor_set_layout.create_set(
	            renderer.descriptor_pool(), {renderer.gbuffer().depth.view(), _gi_specular.view()}))

	  , _blend_renderpass(
	            build_blend_render_pass(renderer, *_descriptor_set_layout, _color_in_out, _blend_framebuffer))
	  , _blend_descriptor_set(
	            _descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                              {renderer.gbuffer().depth.view(0),
	                                               renderer.gbuffer().mat_data.view(0),
	                                               renderer.gbuffer().depth.view(_base_mip_level),
	                                               renderer.gbuffer().mat_data.view(_base_mip_level),
	                                               _gi_diffuse_result.view(),
	                                               _gi_specular_history.view(),
	                                               renderer.gbuffer().albedo_mat_id.view(0),
	                                               _integrated_brdf.view()}))
	{

		auto end = _max_mip_level;
		_sample_descriptor_sets.reserve(gsl::narrow<std::size_t>(end - _min_mip_level));

		for(auto i = 0; i < end - _min_mip_level; i++) {
			auto curr_mip = i + _min_mip_level;
			auto prev_mip = util::min(curr_mip + 1, renderer.gbuffer().depth.mip_levels());

			auto images = {_color_diffuse_in.view(curr_mip),
			               renderer.gbuffer().depth.view(curr_mip),
			               renderer.gbuffer().mat_data.view(curr_mip),
			               _gi_diffuse_result.view(i + 1),
			               _gi_diffuse_samples.view(i),
			               _gi_diffuse_history.view(i),
			               _history_weight.view(i),
			               renderer.gbuffer().depth.view(prev_mip),
			               renderer.gbuffer().mat_data.view(prev_mip),
			               renderer.gbuffer().ambient_occlusion.get_or(_color_diffuse_in).view()};

			_sample_descriptor_sets.emplace_back(
			        _descriptor_set_layout.create_set(renderer.descriptor_pool(), images));
		}
	}


	void Gi_pass::update(util::Time) {}

	void Gi_pass::draw(Frame_data& frame)
	{

		if(!_renderer.settings().gi) {
			_first_frame = true;
			return;
		}

		auto eye_position = glm::vec3(_renderer.global_uniforms().eye_pos.x,
		                              _renderer.global_uniforms().eye_pos.y,
		                              _renderer.global_uniforms().eye_pos.z);

		auto movement      = glm::distance2(eye_position, _prev_eye_position);
		_prev_eye_position = eye_position;

		auto skip_reprojection = _first_frame || movement > 4.f;

		if(skip_reprojection) {
			LOG(plog::debug) << "Teleport detected";

			graphic::clear_texture(frame.main_command_buffer,
			                       _gi_diffuse_history,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal);
			graphic::clear_texture(frame.main_command_buffer,
			                       _gi_diffuse_result,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal);
			graphic::clear_texture(frame.main_command_buffer,
			                       _gi_diffuse_samples,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal);


			for(auto rt : {&_gi_specular, &_gi_specular_history, &_history_weight}) {
				graphic::clear_texture(frame.main_command_buffer,
				                       *rt,
				                       util::Rgba{0, 0, 0, 0},
				                       vk::ImageLayout::eUndefined,
				                       vk::ImageLayout::eShaderReadOnlyOptimal,
				                       0,
				                       1);
			}

			_prev_view = _renderer.global_uniforms().view_mat;
			_prev_proj = _renderer.global_uniforms().proj_mat;
		}

		if(_first_frame) {
			_first_frame = false;

			_integrate_brdf(frame.main_command_buffer);
		}


		_generate_first_mipmaps(frame.main_command_buffer, frame.global_uniform_set);

		if(!skip_reprojection) {
			_reproject_history(frame.main_command_buffer, frame.global_uniform_set);
		}

		_generate_mipmaps(frame.main_command_buffer, frame.global_uniform_set);
		_generate_gi_samples(frame.main_command_buffer, frame.global_uniform_set);
		_draw_gi(frame.main_command_buffer);
	}

	void Gi_pass::_integrate_brdf(vk::CommandBuffer command_buffer)
	{
		_brdf_integration_renderpass.execute(
		        command_buffer, _brdf_integration_framebuffer, [&] { command_buffer.draw(3, 1, 0, 0); });
	}

	namespace {
		bool is_zero(float v)
		{
			constexpr auto epsilon = 0.0000001f;
			return v > -epsilon && v < epsilon;
		}
	} // namespace

	void Gi_pass::_reproject_history(vk::CommandBuffer command_buffer, vk::DescriptorSet globals)
	{
		auto _ = _renderer.profiler().push("Reproject");

		auto pcs         = Gi_constants{};
		pcs.reprojection = _prev_view * glm::inverse(_renderer.global_uniforms().view_mat);
		MIRRAGE_INVARIANT(is_zero(pcs.reprojection[0][3]) && is_zero(pcs.reprojection[1][3])
		                          && is_zero(pcs.reprojection[2][3]) && is_zero(pcs.reprojection[3][3] - 1),
		                  "m[0][3]!=0 or m[1][3]!=0 or m[2][3]!=0 or m[3][3]!=1");

		pcs.reprojection[0][3] = -2.f / _prev_proj[0][0];
		pcs.reprojection[1][3] = -2.f / _prev_proj[1][1];
		pcs.reprojection[2][3] = (1.f - _prev_proj[0][2]) / _prev_proj[0][0];
		pcs.reprojection[3][3] = (1.f + _prev_proj[1][2]) / _prev_proj[1][1];

		pcs.prev_projection = _prev_proj;
		MIRRAGE_INVARIANT(is_zero(pcs.prev_projection[0][3]) && is_zero(pcs.prev_projection[1][3])
		                          && is_zero(pcs.prev_projection[3][3]),
		                  "m[0][3]!=0 or m[1][3]!=0 or m[3][3]!=0");

		pcs.prev_projection[0][3] = float(_min_mip_level);
		pcs.prev_projection[1][3] = float(_max_mip_level - 1);

		// reproject MIP 0
		_reproject_renderpass.execute(command_buffer, _reproject_framebuffer, [&] {
			auto descriptor_sets = std::array<vk::DescriptorSet, 2>{globals, *_reproject_descriptor_set};
			_reproject_renderpass.bind_descriptor_sets(0, descriptor_sets);

			_reproject_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});

		// update weights
		_reproject_weight_renderpass.execute(command_buffer, _reproject_weight_framebuffer, [&] {
			_reproject_weight_renderpass.bind_descriptor_set(1, *_reproject_weight_descriptor_set);
			command_buffer.draw(3, 1, 0, 0);
		});

		// reproject MIP 1-N
		for(auto i = 1; i < _gi_diffuse_history.mip_levels(); i++) {
			auto& fb = _diffuse_reproject_framebuffers[std::size_t(i)];

			_diffuse_reproject_renderpass.execute(command_buffer, fb, [&] {
				if(i == 1)
					_diffuse_reproject_renderpass.bind_descriptor_set(1, *_reproject_descriptor_set);

				pcs.prev_projection[0][3] = float(i);
				_diffuse_reproject_renderpass.push_constant("pcs"_strid, pcs);

				command_buffer.draw(3, 1, 0, 0);
			});
		}

		// generate mipmaps for weights
		for(auto i : util::range(1, std::size_t(_renderer.gbuffer().mip_levels))) {
			// barrier against write to previous mipmap level
			auto subresource = vk::ImageSubresourceRange{
			        vk::ImageAspectFlagBits::eColor, gsl::narrow<std::uint32_t>(i - 1), 1, 0, 1};
			auto barrier = vk::ImageMemoryBarrier{vk::AccessFlagBits::eColorAttachmentWrite,
			                                      vk::AccessFlagBits::eShaderRead,
			                                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                                      VK_QUEUE_FAMILY_IGNORED,
			                                      VK_QUEUE_FAMILY_IGNORED,
			                                      _history_weight.image(),
			                                      subresource};
			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                               vk::PipelineStageFlagBits::eFragmentShader,
			                               vk::DependencyFlags{},
			                               {},
			                               {},
			                               {barrier});

			_weight_mipgen_renderpass.execute(command_buffer, _weight_mipgen_framebuffers.at(i), [&] {
				_weight_mipgen_renderpass.bind_descriptor_set(1, *_weight_mipgen_descriptor_sets.at(i - 1));
				pcs.prev_projection[0][3] = float(i) - float(_diffuse_mip_level);
				_weight_mipgen_renderpass.push_constant("pcs"_strid, pcs);
				command_buffer.draw(3, 1, 0, 0);
			});
		}

		_prev_view = _renderer.global_uniforms().view_mat;
		_prev_proj = _renderer.global_uniforms().proj_mat;
	}

	void Gi_pass::_generate_first_mipmaps(vk::CommandBuffer command_buffer, vk::DescriptorSet)
	{
		if(_min_mip_level > 0) {
			auto _ = _renderer.profiler().push("Gen. Mipmaps A");

			graphic::generate_mipmaps(command_buffer,
			                          _color_diffuse_in.image(),
			                          vk::ImageLayout::eShaderReadOnlyOptimal,
			                          vk::ImageLayout::eShaderReadOnlyOptimal,
			                          _color_diffuse_in.width(),
			                          _color_diffuse_in.height(),
			                          _min_mip_level + 1);
		}
	}
	void Gi_pass::_generate_mipmaps(vk::CommandBuffer command_buffer, vk::DescriptorSet)
	{
		auto _ = _renderer.profiler().push("Gen. Mipmaps B");

		graphic::generate_mipmaps(command_buffer,
		                          _color_diffuse_in.image(),
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          _color_diffuse_in.width(),
		                          _color_diffuse_in.height(),
		                          _renderer.gbuffer().mip_levels,
		                          _min_mip_level);
	}

	namespace {
		float calc_ds_factor(bool  last_sample,
		                     int   samples,
		                     float fov_h,
		                     float fov_v,
		                     float screen_width,
		                     float screen_height,
		                     float proj_y_plane)
		{
			auto dp = glm::pi<float>() * 40.f * 40.f;
			if(last_sample)
				dp -= glm::pi<float>() * 20.f * 20.f;

			dp /= float(samples);

			return (4.0f * glm::tan(fov_h / 2.f) * glm::tan(fov_v / 2.f) * dp)
			       / (screen_width * screen_height) * proj_y_plane * proj_y_plane;
		}
	} // namespace

	void Gi_pass::_generate_gi_samples(vk::CommandBuffer command_buffer, vk::DescriptorSet globals)
	{
		auto base_mip = static_cast<int>(_min_mip_level);
		auto begin    = static_cast<int>(_diffuse_mip_level);
		auto end      = static_cast<int>(_max_mip_level);
		auto last_i   = std::min(base_mip, begin);


		auto pcs                  = Gi_constants{};
		pcs.prev_projection[0][0] = float(_highres_base_mip_level);
		pcs.prev_projection[1][0] = float(end - 1);
		pcs.prev_projection[2][0] = float(std::max(1.f, (end - 1) - last_i - 0.8f));
		pcs.prev_projection[1][3] = float(_max_mip_level - 1);
		pcs.prev_projection[3][3] = float(_min_mip_level);

		if(_renderer.gbuffer().ambient_occlusion.is_some()) {
			pcs.reprojection[3][3] = 1.0;
		} else {
			pcs.reprojection[3][3] = 0.0;
		}

		{
			auto _               = _renderer.profiler().push("Sample (diffuse)");
			auto first_iteration = true;
			for(auto i = end - 1; i >= begin; i--) {
				// render highest MIP to blend buffer, because there is nothing to upsample, yet
				auto& fb = _sample_framebuffers.at(gsl::narrow<std::size_t>(i - base_mip));

				_sample_renderpass.execute(command_buffer, fb, [&] {
					auto sample_count = to_2prod(_renderer.settings().gi_samples);

					if(i == end - 1) {
						_sample_renderpass.set_stage("sample_first"_strid);
						sample_count = to_2prod(_renderer.settings().gi_lowres_samples);
					} else if(i == begin)
						_sample_renderpass.set_stage("sample_last"_strid);
					else
						_sample_renderpass.set_stage("sample"_strid);

					if(first_iteration) {
						_sample_renderpass.bind_descriptor_sets(
						        0,
						        std::array<vk::DescriptorSet, 2>{globals, _renderer.noise_descriptor_set()});
					}

					_sample_renderpass.bind_descriptor_set(
					        2, *_sample_descriptor_sets[std::size_t(i - base_mip)]);

					pcs.prev_projection[0][3] = float(i);
					auto fov_h                = _renderer.global_uniforms().proj_planes.z;
					auto fov_v                = _renderer.global_uniforms().proj_planes.w;

					pcs.prev_projection[2][3] = calc_ds_factor(i == begin,
					                                           sample_count,
					                                           fov_h,
					                                           fov_v,
					                                           float(_color_in_out.width(i)),
					                                           float(_color_in_out.height(i)),
					                                           _renderer.global_uniforms().proj_planes.y);

					auto screen_size = glm::vec2{_color_diffuse_in.width(i), _color_diffuse_in.height(i)};
					auto ndc_to_uv   = glm::translate(glm::mat4(1.f), glm::vec3(screen_size / 2.f, 0.f))
					                 * glm::scale(glm::mat4(1.f), glm::vec3(screen_size / 2.f, 1.f));
					pcs.reprojection = ndc_to_uv * _renderer.global_uniforms().proj_mat;

					_sample_renderpass.push_constant("pcs"_strid, pcs);

					command_buffer.draw(3, 1, 0, 0);
				});

				first_iteration = false;
			}
		}

		{
			auto _ = _renderer.profiler().push("Sample (blend");

			for(auto i = int(_sample_framebuffers.size() - 1); i >= 0; i--) {
				auto& fb_upsample = _sample_framebuffers.at(std::size_t(i));
				auto& fb_final    = _sample_framebuffers_blend.at(std::size_t(i));

				// upsample
				if(i < int(_sample_framebuffers.size() - 1)) {
					_sample_renderpass.execute(command_buffer, fb_upsample, [&] {
						if(i + base_mip > begin)
							_sample_renderpass.set_stage("upsample"_strid);
						else
							_sample_renderpass.set_stage("upsample_np"_strid);

						_sample_renderpass.bind_descriptor_set(2, *_sample_descriptor_sets[std::size_t(i)]);

						command_buffer.draw(3, 1, 0, 0);
					});
				}

				// blend history
				_sample_renderpass.execute(command_buffer, fb_final, [&] {
					if(i > 1)
						_sample_renderpass.set_stage("blend"_strid);
					else
						_sample_renderpass.set_stage("blend_last"_strid);

					_sample_renderpass.bind_descriptor_set(2, *_sample_descriptor_sets[std::size_t(i)]);

					command_buffer.draw(3, 1, 0, 0);
				});
			}
		}

		{
			auto _ = _renderer.profiler().push("Sample (spec)");
			_sample_spec_renderpass.execute(command_buffer, _sample_spec_framebuffer, [&] {
				_sample_spec_renderpass.bind_descriptor_set(1, *_sample_spec_descriptor_set);

				// Mip-level used by spec.
				pcs.prev_projection[0][3] = float(_base_mip_level);

				auto screen_size = glm::vec2{_color_diffuse_in.width(_base_mip_level),
				                             _color_diffuse_in.height(_base_mip_level)};
				auto ndc_to_uv   = glm::translate(glm::mat4(1.f), glm::vec3(screen_size / 2.f, 0.f))
				                 * glm::scale(glm::mat4(1.f), glm::vec3(screen_size / 2.f, 1.f));
				pcs.reprojection = ndc_to_uv * _renderer.global_uniforms().proj_mat;

				_sample_spec_renderpass.push_constant("pcs"_strid, pcs);

				command_buffer.draw(3, 1, 0, 0);
			});

			_median_spec_renderpass.execute(command_buffer, _median_spec_framebuffer, [&] {
				_median_spec_renderpass.bind_descriptor_set(1, *_median_spec_descriptor_set);
				command_buffer.draw(3, 1, 0, 0);
			});
		}


		auto _ = _renderer.profiler().push("Sample (spec blur)");
		for(int i = 0; i < 1; i++) {
			_blur_spec_gi(command_buffer);
		}
	}

	void Gi_pass::_blur_spec_gi(vk::CommandBuffer command_buffer)
	{
		// blur horizontal
		_blur_render_pass.execute(command_buffer, _blur_horizonal_framebuffer, [&] {
			_blur_render_pass.bind_descriptor_set(1, *_blur_descriptor_set_horizontal);
			_blur_render_pass.set_stage("blur_h"_strid);
			command_buffer.draw(3, 1, 0, 0);
		});
		// blur vertical
		_blur_render_pass.execute(command_buffer, _blur_vertical_framebuffer, [&] {
			_blur_render_pass.bind_descriptor_set(1, *_blur_descriptor_set_vertical);
			_blur_render_pass.set_stage("blur_v"_strid);

			command_buffer.draw(3, 1, 0, 0);
		});
	}

	void Gi_pass::_draw_gi(vk::CommandBuffer command_buffer)
	{
		auto _ = _renderer.profiler().push("Combine");

		// blend input into result
		_blend_renderpass.execute(command_buffer, _blend_framebuffer, [&] {
			_blend_renderpass.bind_descriptor_set(1, *_blend_descriptor_set);

			auto pcs                  = Gi_constants{};
			pcs.prev_projection[0][3] = float(_min_mip_level);
			pcs.prev_projection[1][3] = float(_max_mip_level - 1);
			pcs.prev_projection[2][3] = float(_renderer.settings().debug_gi_layer);

			_blend_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});
	}



	auto Gi_pass_factory::create_pass(Deferred_renderer& renderer,
	                                  ecs::Entity_manager&,
	                                  Engine&,
	                                  bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		auto& in_out = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		auto& in_diff = !write_first_pp_buffer ? renderer.gbuffer().colorB : renderer.gbuffer().colorA;

		// writes back to the read texture, so we don't have to flip write_first_pp_buffer

		return std::make_unique<Gi_pass>(renderer, in_out, in_diff);
	}

	auto Gi_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Gi_pass_factory::configure_device(vk::PhysicalDevice,
	                                       util::maybe<std::uint32_t>,
	                                       graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
