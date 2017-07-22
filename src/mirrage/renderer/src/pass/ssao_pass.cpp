#include <mirrage/renderer/pass/ssao_pass.hpp>

#include <mirrage/graphic/window.hpp>
#include <mirrage/utils/math.hpp>

#include <glm/gtx/string_cast.hpp>


namespace mirrage {
namespace renderer {

	using namespace util::unit_literals;

	namespace {
		struct Push_constants {
			glm::vec4 options; // max_mip_level, proj_scale
		};

		auto build_Ssao_render_pass(Deferred_renderer& renderer,
		                            vk::DescriptorSetLayout desc_set_layout,
		                            vk::Format format,
		                            graphic::Render_target_2D& target_buffer,
		                            graphic::Framebuffer& out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto ao = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid, sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(ao);

			pass.stage("ssao"_strid)
			    .shader("frag_shader:ssao"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:ssao"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer({target_buffer.view(0), util::Rgba{1.f, 1.f, 1.f, 0.f}},
				                                        target_buffer.width(),
				                                        target_buffer.height());

			return render_pass;
		}
		auto build_blur_render_pass(Deferred_renderer& renderer,
		                            vk::DescriptorSetLayout desc_set_layout,
		                            vk::Format format,
		                            graphic::Render_target_2D& blur_buffer,
		                            graphic::Render_target_2D& result_buffer,
		                            graphic::Framebuffer& out_blur_framebuffer,
		                            graphic::Framebuffer& out_result_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto ao = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("pcs"_strid, sizeof(Push_constants), vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(ao);

			pass.stage("blur_h"_strid)
			    .shader("frag_shader:ssao_blur"_aid, graphic::Shader_stage::fragment, "main", 0, true)
			    .shader("vert_shader:ssao_blur"_aid, graphic::Shader_stage::vertex,   "main", 0, true);

			pass.stage("blur_v"_strid)
			    .shader("frag_shader:ssao_blur"_aid, graphic::Shader_stage::fragment, "main", 0, false)
			    .shader("vert_shader:ssao_blur"_aid, graphic::Shader_stage::vertex,   "main", 0, false);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);

			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			out_blur_framebuffer = builder.build_framebuffer({blur_buffer.view(0), util::Rgba{}},
				                                              blur_buffer.width(),
				                                              blur_buffer.height());

			out_result_framebuffer = builder.build_framebuffer({result_buffer.view(0), util::Rgba{}},
				                                               result_buffer.width(),
				                                               result_buffer.height());

			return render_pass;
		}

		auto get_ao_format(graphic::Device& device) {
			auto format = device.get_supported_format({vk::Format::eR8G8B8Unorm, vk::Format::eR8G8B8A8Unorm},
			                                          vk::FormatFeatureFlagBits::eColorAttachment
			                                          | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			INVARIANT(format.is_some(), "AO render targets are not supported!");

			return format.get_or_throw();
		}

		constexpr auto ao_mip_level = 1;
	}


	Ssao_pass::Ssao_pass(Deferred_renderer& renderer)
	    : _renderer(renderer)
	    , _ao_format(get_ao_format(renderer.device()))
	    , _ao_result_buffer(renderer.device(),
	                        {renderer.gbuffer().depth.width(ao_mip_level),
	                         renderer.gbuffer().depth.height(ao_mip_level)},
	                        1,
	                        _ao_format,
	                        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
	                        vk::ImageAspectFlagBits::eColor)
	    , _blur_buffer(renderer.device(),
	                   {_ao_result_buffer.width(), _ao_result_buffer.height()}, 1,
	                   _ao_format,
	                   vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
	                   vk::ImageAspectFlagBits::eColor)

	    , _sampler(renderer.device().create_sampler(12, vk::SamplerAddressMode::eClampToEdge,
	                                                vk::BorderColor::eIntOpaqueWhite,
	                                                vk::Filter::eLinear,
	                                                vk::SamplerMipmapMode::eNearest))
	    , _descriptor_set_layout(renderer.device(), *_sampler, 2,
	                             vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex)
	    , _ssao_render_pass(build_Ssao_render_pass(renderer, *_descriptor_set_layout, _ao_format,
	                                               _ao_result_buffer, _ao_result_framebuffer))
	    , _ssao_descriptor_set(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                                             {renderer.gbuffer().depth.view(),
	                                                              renderer.gbuffer().mat_data.view(ao_mip_level)}))
	    , _blur_render_pass(build_blur_render_pass(renderer, *_descriptor_set_layout, _ao_format,
	                                               _blur_buffer, _ao_result_buffer,
	                                               _blur_framebuffer, _ao_result_blur_framebuffer))
	    , _blur_descriptor_set_horizontal(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
                                                                            {renderer.gbuffer().depth.view(),
                                                                             _ao_result_buffer.view(0)}))
	    , _blur_descriptor_set_vertical(_descriptor_set_layout.create_set(renderer.descriptor_pool(),
	                                                                      {renderer.gbuffer().depth.view(),
	                                                                       _blur_buffer.view(0)}))
	{

		INVARIANT(!renderer.gbuffer().ambient_occlusion, "More than one ambient occlusion implementation activ!");
		renderer.gbuffer().ambient_occlusion = _ao_result_buffer;
	}


	void Ssao_pass::update(util::Time dt) {
	}

	void Ssao_pass::draw(vk::CommandBuffer& command_buffer,
	                     Command_buffer_source&,
	                     vk::DescriptorSet global_uniform_set,
	                     std::size_t) {

		auto descriptor_sets = std::array<vk::DescriptorSet, 2> {
			global_uniform_set,
			*_ssao_descriptor_set
		};

		Push_constants pcs;
		pcs.options.x = util::max(1, _renderer.gbuffer().mip_levels - ao_mip_level - 1);

		auto& cam = _renderer.active_camera().get_or_throw();
		float height = _ao_result_buffer.height();
		float v_fov  = cam.fov_vertical.value();
		pcs.options.y = height / (-2.f * glm::tan(v_fov*0.5f));

		pcs.options.z = ao_mip_level;

		// sample ao
		_ssao_render_pass.execute(command_buffer, _ao_result_framebuffer, [&] {
			_ssao_render_pass.bind_descriptor_sets(0, descriptor_sets);
			_ssao_render_pass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});

		for(int i=0; i<2; i++) {
			// blur horizontal
			_blur_render_pass.execute(command_buffer, _blur_framebuffer, [&] {
				_blur_render_pass.bind_descriptor_set(1, *_blur_descriptor_set_horizontal);
				command_buffer.draw(3, 1, 0, 0);
			});
			// blur vertical
			_blur_render_pass.execute(command_buffer, _ao_result_blur_framebuffer, [&] {
				_blur_render_pass.bind_descriptor_set(1, *_blur_descriptor_set_vertical);
				_blur_render_pass.set_stage("blur_v"_strid);

				command_buffer.draw(3, 1, 0, 0);
			});
		}
	}


	auto Ssao_pass_factory::create_pass(Deferred_renderer& renderer,
	                                    ecs::Entity_manager& entities,
	                                    util::maybe<Meta_system&> meta_system,
	                                    bool& write_first_pp_buffer) -> std::unique_ptr<Pass> {
		return std::make_unique<Ssao_pass>(renderer);
	}

	auto Ssao_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
	                                   int current_score) -> int {
		return current_score;
	}

	void Ssao_pass_factory::configure_device(vk::PhysicalDevice,
	                                        util::maybe<std::uint32_t>,
	                                        graphic::Device_create_info&) {
	}


}
}
