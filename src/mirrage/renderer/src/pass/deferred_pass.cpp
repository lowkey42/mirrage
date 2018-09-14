#include <mirrage/renderer/pass/deferred_pass.hpp>

#include <mirrage/renderer/light_comp.hpp>

#include <mirrage/graphic/render_pass.hpp>


using namespace mirrage::graphic;

namespace mirrage::renderer {

	namespace {
		auto build_render_pass(Deferred_renderer&         renderer,
		                       graphic::Render_target_2D& color_target,
		                       graphic::Render_target_2D& color_target_diff,
		                       graphic::Render_target_2D& depth_buffer,
		                       vk::Format                 depth_buffer_format,
		                       Deferred_geometry_subpass& gpass,
		                       Deferred_lighting_subpass& lpass,
		                       graphic::Framebuffer&      out_framebuffer)
		{
			auto  builder = renderer.device().create_render_pass_builder();
			auto& gbuffer = renderer.gbuffer();

			auto depth = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  depth_buffer_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

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
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{};
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
			                                       .depth_stencil_attachment(depth);
			gpass.configure_animation_subpass(renderer, animated_geometry_pass);


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
			                .input_attachment(mat_data);
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

			auto attachments = std::array<Framebuffer_attachment_desc, 6>{
			        {{depth_buffer.view(0), 1.f},
			         {gbuffer.depth.view(0), util::Rgba(1.f)},
			         {gbuffer.albedo_mat_id.view(0), sky_color},
			         {gbuffer.mat_data.view(0), util::Rgba{0, 0, 1, 0}},
			         {color_target.view(0), util::Rgba{0, 0, 0, 0}},
			         {color_target_diff.view(0), util::Rgba{0, 0, 0, 0}}}};
			out_framebuffer =
			        builder.build_framebuffer(attachments, color_target.width(), color_target.height());

			return render_pass;
		}
	} // namespace

	Deferred_pass::Deferred_pass(Deferred_renderer&         renderer,
	                             ecs::Entity_manager&       entities,
	                             graphic::Render_target_2D& color_target,
	                             graphic::Render_target_2D& color_target_diff)
	  : _renderer(renderer)
	  , _depth(renderer.device(),
	           {color_target.width(), color_target.height()},
	           1,
	           renderer.device().get_depth_format(),
	           vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
	           vk::ImageAspectFlagBits::eDepth)
	  , _gpass(renderer, entities)
	  , _lpass(renderer, entities, _depth)
	  , _render_pass(build_render_pass(renderer,
	                                   color_target,
	                                   color_target_diff,
	                                   _depth,
	                                   renderer.device().get_depth_format(),
	                                   _gpass,
	                                   _lpass,
	                                   _gbuffer_framebuffer))
	{
	}


	void Deferred_pass::update(util::Time dt)
	{
		_gpass.update(dt);
		_lpass.update(dt);
	}
	void Deferred_pass::draw(Frame_data& frame)
	{

		if(!_first_frame) {
			graphic::blit_texture(frame.main_command_buffer,
			                      _renderer.gbuffer().depth,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _renderer.gbuffer().prev_depth,
			                      vk::ImageLayout::eUndefined,
			                      vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		_gpass.pre_draw(frame);

		_render_pass.execute(frame.main_command_buffer, _gbuffer_framebuffer, [&] {
			_render_pass.bind_descriptor_sets(0, {&frame.global_uniform_set, 1});

			_gpass.draw(frame, _render_pass);

			_render_pass.next_subpass(true);

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

	auto Deferred_pass_factory::create_pass(Deferred_renderer&   renderer,
	                                        ecs::Entity_manager& entities,
	                                        Engine&,
	                                        bool& use_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		auto& color_target = use_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		auto& color_target_diff = use_first_pp_buffer ? renderer.gbuffer().colorB : renderer.gbuffer().colorA;

		use_first_pp_buffer = !use_first_pp_buffer;
		return std::make_unique<Deferred_pass>(renderer, entities, color_target, color_target_diff);
	}

	auto Deferred_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Deferred_pass_factory::configure_device(vk::PhysicalDevice,
	                                             util::maybe<std::uint32_t>,
	                                             graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
