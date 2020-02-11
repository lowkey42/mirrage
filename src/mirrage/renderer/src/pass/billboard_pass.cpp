#include <mirrage/renderer/pass/billboard_pass.hpp>


namespace mirrage::renderer {

	namespace {
		auto build_render_pass(Deferred_renderer&         renderer,
		                       graphic::Render_target_2D& target,
		                       graphic::Framebuffer&      out_framebuffer)
		{

			auto  builder = renderer.device().create_render_pass_builder();
			auto& gbuffer = renderer.gbuffer();

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

			auto screen = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  gbuffer.color_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eLoad,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleStrip;
			pipeline.rasterization.cullMode  = vk::CullModeFlagBits::eNone;
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{
                    vk::PipelineDepthStencilStateCreateFlags{}, true, false, vk::CompareOp::eLess};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(renderer.model_descriptor_set_layout());

			pipeline.add_push_constant("dpc"_strid,
			                           sizeof(Billboard_push_constants),
			                           vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);

			auto& pass =
			        builder.add_subpass(pipeline).color_attachment(screen).depth_stencil_attachment(depth);

			pass.stage("default"_strid)
			        .shader("frag_shader:billboard_unlit"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:billboard"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			auto attachments = std::array<graphic::Framebuffer_attachment_desc, 2>{
			        {{renderer.gbuffer().depth_buffer.view(0), 1.f}, {target.view(0), util::Rgba{}}}};

			out_framebuffer = builder.build_framebuffer(attachments, target.width(), target.height());

			return render_pass;
		}
	} // namespace


	Billboard_pass::Billboard_pass(Deferred_renderer&         renderer,
	                               ecs::Entity_manager&       entities,
	                               graphic::Render_target_2D& src)
	  : Render_pass(renderer)
	  , _entities(entities)
	  , _render_pass(build_render_pass(renderer, src, _framebuffer))
	{
	}


	void Billboard_pass::update(util::Time) {}

	void Billboard_pass::pre_draw(Frame_data&) { _queue.clear(); }

	void Billboard_pass::handle_obj(Frame_data&      frame,
	                                Culling_mask     mask,
	                                const Billboard& bb,
	                                const glm::vec3& pos)
	{
		if((mask & frame.camera_culling_mask) != 0 && !bb.dynamic_lighting) {
			_queue.emplace_back(bb);
			_queue.back().offset += pos;
		}
	}

	void Billboard_pass::post_draw(Frame_data& frame)
	{
		auto _ = _mark_subpass(frame);

		if(_queue.empty())
			return;

		std::sort(_queue.begin(), _queue.end(), [](auto& lhs, auto& rhs) {
			return &*lhs.material < &*rhs.material;
		});

		_render_pass.execute(frame.main_command_buffer, _framebuffer, [&] {
			_render_pass.bind_descriptor_set(0, frame.global_uniform_set);

			auto last_material = static_cast<const Material*>(nullptr);
			for(auto&& bb : _queue) {
				if(&*bb.material != last_material) {
					last_material = &*bb.material;
					last_material->bind(_render_pass);
				}

				auto pcs = construct_push_constants(bb,
				                                    glm::vec3(0, 0, 0),
				                                    _renderer.global_uniforms().view_mat,
				                                    _renderer.window().viewport());

				_render_pass.push_constant("dpc"_strid, pcs);
				frame.main_command_buffer.draw(4, 1, 0, 0);
			}
		});
	}


	auto Billboard_pass_factory::create_pass(Deferred_renderer& renderer,
	                                         std::shared_ptr<void>,
	                                         util::maybe<ecs::Entity_manager&> entities,
	                                         Engine&,
	                                         bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		auto& color_src = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;

		return std::make_unique<Billboard_pass>(
		        renderer, entities.get_or_throw("Deferred_pass requires an entitymanager."), color_src);
	}

	auto Billboard_pass_factory::rank_device(vk::PhysicalDevice,
	                                         util::maybe<std::uint32_t>,
	                                         int current_score) -> int
	{
		return current_score;
	}

	void Billboard_pass_factory::configure_device(vk::PhysicalDevice,
	                                              util::maybe<std::uint32_t>,
	                                              graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
