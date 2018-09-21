#include <mirrage/renderer/pass/debug_draw_pass.hpp>


namespace mirrage::renderer {

	namespace {
		struct Debug_vertex {
			glm::vec3 position;
			util::Rgb color;
		};

		auto build_render_pass(Deferred_renderer&         renderer,
		                       graphic::Render_target_2D& color_target,
		                       graphic::Framebuffer&      out_framebuffer)
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
			auto depth = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  renderer.gbuffer().depth_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eLoad,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eLineList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{
                    vk::PipelineDepthStencilStateCreateFlags{}, true, false, vk::CompareOp::eLess};
			pipeline.rasterization.cullMode                = vk::CullModeFlagBits::eNone;
			pipeline.rasterization.depthBiasEnable         = true;
			pipeline.rasterization.depthBiasClamp          = 0.f;
			pipeline.rasterization.depthBiasConstantFactor = 0.01f;

			if(renderer.device().physical_device().getFeatures().wideLines) {
				pipeline.rasterization.lineWidth = 4.f;
			}

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());

			pipeline.vertex<Debug_vertex>(0, false, 0, &Debug_vertex::position, 1, &Debug_vertex::color);

			auto& pass =
			        builder.add_subpass(pipeline).color_attachment(color).depth_stencil_attachment(depth);

			pass.stage("draw"_strid)
			        .shader("frag_shader:debug_draw"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:debug_draw"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			auto attachments = std::array<graphic::Framebuffer_attachment_desc, 2>{
			        {{color_target.view(0), 1.f},
			         {renderer.gbuffer().depth_buffer.view(0), util::Rgba(1.f)}}};
			out_framebuffer =
			        builder.build_framebuffer(attachments, color_target.width(), color_target.height());

			return render_pass;
		}
	} // namespace


	Debug_draw_pass::Debug_draw_pass(Deferred_renderer& renderer, graphic::Render_target_2D& src)
	  : _renderer(renderer)
	  , _vertices(renderer.device(), 64 * sizeof(Debug_vertex), vk::BufferUsageFlagBits::eVertexBuffer)
	  , _render_pass(build_render_pass(renderer, src, _framebuffer))
	{
	}


	void Debug_draw_pass::update(util::Time) {}

	void Debug_draw_pass::draw(Frame_data& frame)
	{
		if(frame.debug_geometry_queue.empty())
			return;

		auto vert_count = gsl::narrow<std::uint32_t>(frame.debug_geometry_queue.size() * 2);
		_vertices.resize(std::int32_t(vert_count * sizeof(Debug_vertex)));

		_vertices.update_objects<Debug_vertex>(0, [&](gsl::span<Debug_vertex> out) {
			auto i = 0;
			for(auto& g : frame.debug_geometry_queue) {
				out[i++] = {g.start, g.color};
				out[i++] = {g.end, g.color};
			}
		});

		_vertices.flush(frame.main_command_buffer,
		                vk::PipelineStageFlagBits::eVertexInput,
		                vk::AccessFlagBits::eVertexAttributeRead);

		_render_pass.execute(frame.main_command_buffer, _framebuffer, [&] {
			_render_pass.bind_descriptor_set(0, frame.global_uniform_set);

			auto buffer = _vertices.read_buffer();
			auto offset = vk::DeviceSize(0);
			frame.main_command_buffer.bindVertexBuffers(0, 1, &buffer, &offset);
			frame.main_command_buffer.draw(vert_count, 1, 0, 0);
		});
	}


	auto Debug_draw_pass_factory::create_pass(Deferred_renderer& renderer,
	                                          ecs::Entity_manager&,
	                                          Engine&,
	                                          bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		auto& color_src = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		return std::make_unique<Debug_draw_pass>(renderer, color_src);
	}

	auto Debug_draw_pass_factory::rank_device(vk::PhysicalDevice,
	                                          util::maybe<std::uint32_t>,
	                                          int current_score) -> int
	{
		return current_score;
	}

	void Debug_draw_pass_factory::configure_device(vk::PhysicalDevice dev,
	                                               util::maybe<std::uint32_t>,
	                                               graphic::Device_create_info& dci)
	{
		dci.features.wideLines = dev.getFeatures().wideLines;
	}
} // namespace mirrage::renderer
