#include <mirrage/renderer/pass/deferred_pass.hpp>

#include <mirrage/renderer/light_comp.hpp>

#include <mirrage/graphic/render_pass.hpp>


using namespace mirrage::graphic;

namespace mirrage::renderer {

	namespace {
		glm::vec2 oct_wrap(float x, float y)
		{
			return glm::fma(glm::step(glm::vec2(0.f), glm::vec2(x, y)), glm::vec2(2.f), glm::vec2(-1.f));
		}
		glm::vec2 encode_normal(glm::vec3 n)
		{
			auto l = glm::dot(glm::abs(n), glm::vec3(1.f));
			n.x /= l;
			n.y /= l;
			return mix(glm::vec2(n.x, n.y),
			           (glm::vec2(1.0f) - glm::abs(glm::vec2(n.y, n.x))) * oct_wrap(n.x, n.y),
			           glm::step(n.z, 0.f))
			               * 0.5f
			       + 0.5f;
		}

		auto build_render_pass(Deferred_renderer&         renderer,
		                       graphic::Render_target_2D& color_target,
		                       graphic::Render_target_2D& color_target_diff,
		                       graphic::Render_target_2D& depth_buffer,
		                       Deferred_geometry_subpass& gpass,
		                       Deferred_lighting_subpass& lpass,
		                       graphic::Framebuffer&      out_framebuffer)
		{
			auto  builder = renderer.device().create_render_pass_builder();
			auto& gbuffer = renderer.gbuffer();

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

			auto depth_sampleable = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  gbuffer.depth_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto albedo_mat_id = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  gbuffer.albedo_mat_id_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto mat_data = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  gbuffer.mat_data_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto color = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  gbuffer.color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto color_diffuse = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  gbuffer.color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                    = Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());

			pipeline.add_push_constant("dpc"_strid,
			                           sizeof(Deferred_push_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);


			auto geometry_pipeline          = pipeline;
			geometry_pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
			        vk::PipelineDepthStencilStateCreateFlags{}, true, true, vk::CompareOp::eLess};
			gpass.configure_pipeline(renderer, geometry_pipeline);
			auto& geometry_pass = builder.add_subpass(geometry_pipeline)
			                              .color_attachment(depth_sampleable)
			                              .color_attachment(albedo_mat_id)
			                              .color_attachment(mat_data)
			                              .preserve_attachment(color)
			                              .preserve_attachment(color_diffuse)
			                              .depth_stencil_attachment(depth);
			gpass.configure_subpass(renderer, geometry_pass);


			auto animated_geometry_pipeline          = pipeline;
			animated_geometry_pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
			        vk::PipelineDepthStencilStateCreateFlags{}, true, true, vk::CompareOp::eLess};
			gpass.configure_animation_pipeline(renderer, animated_geometry_pipeline);
			auto& animated_geometry_pass = builder.add_subpass(animated_geometry_pipeline)
			                                       .color_attachment(depth_sampleable)
			                                       .color_attachment(albedo_mat_id)
			                                       .color_attachment(mat_data)
			                                       .preserve_attachment(color)
			                                       .preserve_attachment(color_diffuse)
			                                       .depth_stencil_attachment(depth);
			gpass.configure_animation_subpass(renderer, animated_geometry_pass);


			auto geometry_emissive_pipeline          = pipeline;
			geometry_emissive_pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
			        vk::PipelineDepthStencilStateCreateFlags{}, true, false, vk::CompareOp::eEqual};
			gpass.configure_pipeline(renderer, geometry_emissive_pipeline);
			auto& geometry_emissive_pass = builder.add_subpass(geometry_emissive_pipeline)
			                                       .color_attachment(color)
			                                       .color_attachment(color_diffuse)
			                                       .preserve_attachment(depth_sampleable)
			                                       .preserve_attachment(albedo_mat_id)
			                                       .preserve_attachment(mat_data)
			                                       .depth_stencil_attachment(depth);
			gpass.configure_emissive_subpass(renderer, geometry_emissive_pass);

			auto animated_geometry_emissive_pipeline          = pipeline;
			animated_geometry_emissive_pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
			        vk::PipelineDepthStencilStateCreateFlags{}, true, false, vk::CompareOp::eEqual};
			gpass.configure_animation_pipeline(renderer, animated_geometry_emissive_pipeline);
			auto& animated_geometry_emissive_pass = builder.add_subpass(animated_geometry_emissive_pipeline)
			                                                .color_attachment(color)
			                                                .color_attachment(color_diffuse)
			                                                .preserve_attachment(depth_sampleable)
			                                                .preserve_attachment(albedo_mat_id)
			                                                .preserve_attachment(mat_data)
			                                                .depth_stencil_attachment(depth);
			gpass.configure_animation_emissive_subpass(renderer, animated_geometry_emissive_pass);


			auto decal_pipeline = pipeline;
			gpass.configure_decal_pipeline(renderer, decal_pipeline);
			auto constant_blend = renderer.device().physical_device_features().independentBlend
			                              ? graphic::Attachment_blend{vk::BlendFactor::eConstantColor,
			                                                          vk::BlendFactor::eOneMinusConstantColor,
			                                                          vk::BlendOp::eAdd,
			                                                          vk::BlendFactor::eConstantColor,
			                                                          vk::BlendFactor::eOneMinusConstantColor,
			                                                          vk::BlendOp::eAdd}
			                              : graphic::blend_premultiplied_alpha;
			auto& decal_pass =
			        builder.add_subpass(decal_pipeline)
			                .input_attachment(depth_sampleable)
			                .color_attachment(albedo_mat_id,
			                                  vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
			                                          | vk::ColorComponentFlagBits::eB,
			                                  graphic::blend_premultiplied_alpha)
			                .color_attachment(mat_data, graphic::all_color_components, constant_blend)
			                .color_attachment(
			                        color, graphic::all_color_components, graphic::blend_premultiplied_alpha)
			                .color_attachment(color_diffuse,
			                                  graphic::all_color_components,
			                                  graphic::blend_premultiplied_alpha)
			                .depth_stencil_attachment(depth);
			gpass.configure_decal_subpass(renderer, decal_pass);


			auto billboard_pipeline          = pipeline;
			billboard_pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
			        vk::PipelineDepthStencilStateCreateFlags{}, true, true, vk::CompareOp::eLess};
			gpass.configure_billboard_pipeline(renderer, billboard_pipeline);
			auto& billboard_pass = builder.add_subpass(billboard_pipeline)
			                               .color_attachment(depth_sampleable)
			                               .color_attachment(albedo_mat_id)
			                               .color_attachment(mat_data)
			                               .color_attachment(color)
			                               .color_attachment(color_diffuse)
			                               .depth_stencil_attachment(depth);
			gpass.configure_billboard_subpass(renderer, billboard_pass);

			auto particle_pipeline          = pipeline;
			particle_pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
			        vk::PipelineDepthStencilStateCreateFlags{}, true, true, vk::CompareOp::eLessOrEqual};
			gpass.configure_particle_pipeline(renderer, particle_pipeline);
			auto& particle_pass = builder.add_subpass(particle_pipeline)
			                              .color_attachment(depth_sampleable)
			                              .color_attachment(albedo_mat_id)
			                              .color_attachment(mat_data)
			                              .color_attachment(color)
			                              .color_attachment(color_diffuse)
			                              .depth_stencil_attachment(depth);
			gpass.configure_particle_subpass(renderer, particle_pass);


			auto light_pipeline    = pipeline;
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};
			lpass.configure_pipeline(renderer, light_pipeline);
			auto& light_pass =
			        builder.add_subpass(light_pipeline)
			                .color_attachment(color, graphic::all_color_components, graphic::blend_add)
			                .color_attachment(
			                        color_diffuse, graphic::all_color_components, graphic::blend_add)
			                .input_attachment(depth_sampleable)
			                .input_attachment(albedo_mat_id)
			                .input_attachment(mat_data)
			                .depth_stencil_attachment(depth);
			lpass.configure_subpass(renderer, light_pass);

			builder.add_dependency(geometry_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       animated_geometry_pass,
			                       vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

			builder.add_dependency(animated_geometry_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       geometry_emissive_pass,
			                       vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

			builder.add_dependency(geometry_emissive_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       animated_geometry_emissive_pass,
			                       vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

			builder.add_dependency(animated_geometry_emissive_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       decal_pass,
			                       vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

			builder.add_dependency(decal_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       billboard_pass,
			                       vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

			builder.add_dependency(billboard_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       particle_pass,
			                       vk::PipelineStageFlagBits::eFragmentShader
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests,
			                       vk::AccessFlagBits::eInputAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite);

			builder.add_dependency(particle_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       light_pass,
			                       vk::PipelineStageFlagBits::eFragmentShader
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests,
			                       vk::AccessFlagBits::eInputAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite);

			auto render_pass = builder.build();

			auto sky_color = util::Rgba{temperature_to_color(25000.f), 1};
			sky_color *= renderer.settings().background_intensity;

			const auto def_normal  = encode_normal(glm::vec3(0, 0, 1));
			auto       attachments = std::array<Framebuffer_attachment_desc, 6>{
                    {{depth_buffer.view(0), 1.f},
                     {gbuffer.depth.view(0), util::Rgba(1.f)},
                     {gbuffer.albedo_mat_id.view(0), util::Rgba{0, 0, 0, 0}},
                     {gbuffer.mat_data.view(0), util::Rgba{def_normal.x, def_normal.y, 1, 0}},
                     {color_target.view(0), sky_color},
                     {color_target_diff.view(0), sky_color}}};
			out_framebuffer =
			        builder.build_framebuffer(attachments, color_target.width(), color_target.height());

			return render_pass;
		}
	} // namespace

	Deferred_pass::Deferred_pass(Deferred_renderer&         renderer,
	                             ecs::Entity_manager&       entities,
	                             graphic::Render_target_2D& color_target,
	                             graphic::Render_target_2D& color_target_diff)
	  : Render_pass(renderer)
	  , _gpass(renderer, entities)
	  , _lpass(renderer, entities, renderer.gbuffer().depth_buffer)
	  , _render_pass(build_render_pass(renderer,
	                                   color_target,
	                                   color_target_diff,
	                                   renderer.gbuffer().depth_buffer,
	                                   _gpass,
	                                   _lpass,
	                                   _gbuffer_framebuffer))
	{

		_gpass.on_render_pass_configured(_render_pass, _gbuffer_framebuffer);
		_lpass.on_render_pass_configured(_render_pass, _gbuffer_framebuffer);
	}


	void Deferred_pass::update(util::Time dt)
	{
		_gpass.update(dt);
		_lpass.update(dt);
	}

	void Deferred_pass::post_draw(Frame_data& frame)
	{
		auto _ = _mark_subpass(frame);

		if(!_first_frame) {
			graphic::blit_texture(frame.main_command_buffer,
			                      _renderer.gbuffer().depth,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _renderer.gbuffer().prev_depth,
			                      vk::ImageLayout::eUndefined,
			                      vk::ImageLayout::eShaderReadOnlyOptimal);
		} else {
			graphic::clear_texture(frame.main_command_buffer,
			                       _renderer.gbuffer().colorA,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal,
			                       0,
			                       _renderer.gbuffer().mip_levels);

			graphic::clear_texture(frame.main_command_buffer,
			                       _renderer.gbuffer().colorB,
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal,
			                       0,
			                       _renderer.gbuffer().mip_levels);
		}

		_render_pass.execute(frame.main_command_buffer,
		                     _gbuffer_framebuffer,
		                     vk::SubpassContents::eSecondaryCommandBuffers,
		                     [&] {
			                     _gpass.draw(frame, _render_pass);
			                     _render_pass.next_subpass();
			                     _render_pass.set_view(frame.main_command_buffer, _gbuffer_framebuffer);
			                     _lpass.draw(frame, _render_pass);
		                     });

		if(_first_frame) {
			_first_frame = false;

			graphic::blit_texture(frame.main_command_buffer,
			                      _renderer.gbuffer().depth,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _renderer.gbuffer().prev_depth,
			                      vk::ImageLayout::eUndefined,
			                      vk::ImageLayout::eShaderReadOnlyOptimal);
		}
	}

	auto Deferred_pass_factory::create_pass(Deferred_renderer& renderer,
	                                        std::shared_ptr<void>,
	                                        util::maybe<ecs::Entity_manager&> entities,
	                                        Engine&,
	                                        bool& use_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		auto& color_target = use_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		auto& color_target_diff = use_first_pp_buffer ? renderer.gbuffer().colorB : renderer.gbuffer().colorA;

		use_first_pp_buffer = !use_first_pp_buffer;
		return std::make_unique<Deferred_pass>(
		        renderer,
		        entities.get_or_throw("Deferred_pass requires an entitymanager."),
		        color_target,
		        color_target_diff);
	}

	auto Deferred_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Deferred_pass_factory::configure_device(vk::PhysicalDevice pd,
	                                             util::maybe<std::uint32_t>,
	                                             graphic::Device_create_info& ci)
	{
		auto features = pd.getFeatures();
		if(features.independentBlend) {
			ci.features.independentBlend = true;
		} else {
			LOG(plog::warning) << "Feature independentBlend is not supported. Decals might not work "
			                      "correctly";
		}
	}
} // namespace mirrage::renderer
