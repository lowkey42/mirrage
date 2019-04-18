#include <mirrage/renderer/pass/depth_of_field_pass.hpp>

#include <mirrage/graphic/window.hpp>


namespace mirrage::renderer {

	using namespace graphic;


	namespace {
		struct Push_constants {
			glm::vec4 arguments{1, 1, 0, 0}; //< focus, range, power
			glm::mat4 depth_reprojection = glm::mat4(1);
		};

		auto build_render_pass(Deferred_renderer&         renderer,
		                       graphic::Render_target_2D& target,
		                       int                        mip_level,
		                       const asset::AID&          shader,
		                       vk::DescriptorSetLayout    desc_set_layout,
		                       Framebuffer&               out_framebuffer)
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
			pipeline.add_push_constant(
			        "pcs"_strid, sizeof(Push_constants), vk::ShaderStageFlagBits::eFragment);


			auto& pass = builder.add_subpass(pipeline).color_attachment(color);

			pass.stage("coc"_strid)
			        .shader(shader, graphic::Shader_stage::fragment)
			        .shader("vert_shader:depth_of_field"_aid, graphic::Shader_stage::vertex);

			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer(
			        Framebuffer_attachment_desc{target.view(mip_level), util::Rgba{}},
			        target.width(mip_level),
			        target.height(mip_level));

			return render_pass;
		}
	} // namespace


	Depth_of_field_pass::Depth_of_field_pass(Deferred_renderer&         renderer,
	                                         graphic::Render_target_2D& src,
	                                         graphic::Render_target_2D& target)
	  : _renderer(renderer)
	  , _src(src)
	  , _target(target)
	  , _gbuffer_sampler(renderer.device().create_sampler(renderer.gbuffer().mip_levels,
	                                                      vk::SamplerAddressMode::eClampToEdge,
	                                                      vk::BorderColor::eIntOpaqueBlack,
	                                                      vk::Filter::eLinear,
	                                                      vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(renderer.device(), *_gbuffer_sampler, 2)

	  , _coc_descriptor_set(_descriptor_set_layout.create_set(
	            renderer.descriptor_pool(), {src.view(0), renderer.gbuffer().depth.view(0)}))
	  , _coc_renderpass(build_render_pass(renderer,
	                                      src,
	                                      1,
	                                      "frag_shader:depth_of_field_coc"_aid,
	                                      *_descriptor_set_layout,
	                                      _coc_framebuffer))

	  , _dof_descriptor_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(), {src.view(2)}))
	  , _dof_renderpass(build_render_pass(renderer,
	                                      target,
	                                      2,
	                                      "frag_shader:depth_of_field_calc"_aid,
	                                      *_descriptor_set_layout,
	                                      _dof_framebuffer))

	  , _apply_descriptor_set(
	            _descriptor_set_layout.create_set(renderer.descriptor_pool(), {src.view(0), target.view(2)}))
	  , _apply_renderpass(build_render_pass(renderer,
	                                        target,
	                                        0,
	                                        "frag_shader:depth_of_field_apply"_aid,
	                                        *_descriptor_set_layout,
	                                        _apply_framebuffer))
	{
	}


	void Depth_of_field_pass::update(util::Time) {}

	void Depth_of_field_pass::draw(Frame_data& frame)
	{
		auto pcs = Push_constants{};
		_renderer.active_camera().process([&](auto& camera) {
			pcs.arguments.r = camera.dof_focus;
			pcs.arguments.g = camera.dof_range;
			pcs.arguments.b = camera.dof_power;

			pcs.depth_reprojection = camera.projection * glm::inverse(camera.pure_projection);
		});

		if(pcs.arguments.b <= 0.0001f || !_renderer.settings().depth_of_field) {
			graphic::blit_texture(frame.main_command_buffer,
			                      _src,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _target,
			                      vk::ImageLayout::eUndefined,
			                      vk::ImageLayout::eShaderReadOnlyOptimal);
			return;
		}

		{
			auto _ = _renderer.profiler().push("Calc CoC");

			_coc_renderpass.execute(frame.main_command_buffer, _coc_framebuffer, [&] {
				auto descriptor_sets =
				        std::array<vk::DescriptorSet, 2>{frame.global_uniform_set, *_coc_descriptor_set};
				_coc_renderpass.bind_descriptor_sets(0, descriptor_sets);

				_coc_renderpass.push_constant("pcs"_strid, pcs);
				frame.main_command_buffer.draw(3, 1, 0, 0);
			});

			graphic::blit_texture(frame.main_command_buffer,
			                      _src,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      _src,
			                      vk::ImageLayout::eUndefined,
			                      vk::ImageLayout::eShaderReadOnlyOptimal,
			                      1,
			                      2);
		}

		{
			auto _ = _renderer.profiler().push("Calc DoF");
			_dof_renderpass.execute(frame.main_command_buffer, _dof_framebuffer, [&] {
				auto descriptor_sets =
				        std::array<vk::DescriptorSet, 2>{frame.global_uniform_set, *_dof_descriptor_set};
				_dof_renderpass.bind_descriptor_sets(0, descriptor_sets);

				_dof_renderpass.push_constant("pcs"_strid, pcs);
				frame.main_command_buffer.draw(3, 1, 0, 0);
			});
		}

		auto _ = _renderer.profiler().push("Apply");

		_apply_renderpass.execute(frame.main_command_buffer, _apply_framebuffer, [&] {
			_apply_renderpass.bind_descriptor_set(1, *_apply_descriptor_set);

			_apply_renderpass.push_constant("pcs"_strid, pcs);
			frame.main_command_buffer.draw(3, 1, 0, 0);
		});
	}


	auto Depth_of_field_pass_factory::create_pass(Deferred_renderer& renderer,
	                                              std::shared_ptr<void>,
	                                              util::maybe<ecs::Entity_manager&>,
	                                              Engine&,
	                                              bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass>
	{
		if(!renderer.settings().depth_of_field)
			return {};

		auto& src    = !write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;
		auto& target = write_first_pp_buffer ? renderer.gbuffer().colorA : renderer.gbuffer().colorB;
		write_first_pp_buffer = !write_first_pp_buffer;

		return std::make_unique<Depth_of_field_pass>(renderer, src, target);
	}

	auto Depth_of_field_pass_factory::rank_device(vk::PhysicalDevice,
	                                              util::maybe<std::uint32_t>,
	                                              int current_score) -> int
	{
		return current_score;
	}

	void Depth_of_field_pass_factory::configure_device(vk::PhysicalDevice,
	                                                   util::maybe<std::uint32_t>,
	                                                   graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
