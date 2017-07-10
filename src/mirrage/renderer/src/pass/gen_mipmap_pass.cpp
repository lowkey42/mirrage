#include <mirrage/renderer/pass/gen_mipmap_pass.hpp>

#include <mirrage/graphic/window.hpp>


namespace lux {
namespace renderer {
	
	using namespace graphic;
	

	namespace {
		struct Push_constants {
			glm::vec4 arguments; //< current level, max level
		};

		auto build_mip_render_pass(Deferred_renderer& renderer,
		                           vk::DescriptorSetLayout desc_set_layout,
		                           std::vector<Framebuffer>& out_framebuffers) {
			
			auto builder = renderer.device().create_render_pass_builder();

			auto depth = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().depth_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});
			
			auto mat_data = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().mat_data_format,
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
			                           vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment);
			
			
			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(depth)
			                    .color_attachment(mat_data);
			
			pass.stage("mipgen"_strid)
			    .shader("frag_shader:gi_mipgen"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:gi_mipgen"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead,
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			for(auto i=1u; i<renderer.gbuffer().mip_levels; i++) {
				auto attachments = std::array<Framebuffer_attachment_desc, 2> {{
				    {renderer.gbuffer().depth.view(i), util::Rgba{}},
				    {renderer.gbuffer().mat_data.view(i), util::Rgba{}}
				}};
				
				out_framebuffers.emplace_back( builder.build_framebuffer(
				        attachments,
				        renderer.gbuffer().depth.width(i),
				        renderer.gbuffer().depth.height(i) ));
				
			}
			
			return render_pass;
		}

		auto create_descriptor_set_layout(graphic::Device& device,
		                                  vk::Sampler sampler) -> vk::UniqueDescriptorSetLayout {
			auto bindings = std::array<vk::DescriptorSetLayoutBinding, 2>();

			for(auto i = std::uint32_t(0); i<bindings.size(); i++) {
				bindings[i] = vk::DescriptorSetLayoutBinding {
				                i, vk::DescriptorType::eCombinedImageSampler,
				                1,
				                vk::ShaderStageFlagBits::eFragment,
				                &sampler};
			}

			return device.create_descriptor_set_layout(bindings);
		}
	}


	Gen_mipmap_pass::Gen_mipmap_pass(Deferred_renderer& renderer)
	    : _renderer(renderer)
	    , _gubffer_sampler(renderer.device().create_sampler(
	                       renderer.gbuffer().mip_levels, vk::SamplerAddressMode::eClampToBorder,
	                       vk::BorderColor::eIntOpaqueBlack,
	                       vk::Filter::eLinear,
	                       vk::SamplerMipmapMode::eNearest))
	    , _descriptor_set_layout(create_descriptor_set_layout(renderer.device(), *_gubffer_sampler))
	    , _descriptor_set(renderer.create_descriptor_set(*_descriptor_set_layout))
	    
	    , _mipmap_gen_renderpass(build_mip_render_pass(renderer,
	                                                   *_descriptor_set_layout,
	                                                   _mipmap_gen_framebuffers)) {

		auto desc_images = std::array<vk::DescriptorImageInfo, 2>{};
		desc_images[0] = vk::DescriptorImageInfo{*_gubffer_sampler, renderer.gbuffer().depth.view(),
		                                         vk::ImageLayout::eShaderReadOnlyOptimal};

		desc_images[1] = vk::DescriptorImageInfo{*_gubffer_sampler, renderer.gbuffer().mat_data.view(),
		                                         vk::ImageLayout::eShaderReadOnlyOptimal};

		auto desc_writes = std::array<vk::WriteDescriptorSet, 2>();
		for(auto i = std::uint32_t(0); i<desc_writes.size(); i++) {
			desc_writes[i] =  vk::WriteDescriptorSet{*_descriptor_set, i, 0,
			                                         1,
			                                         vk::DescriptorType::eCombinedImageSampler,
			                                         &desc_images[i], nullptr};
		}

		renderer.device().vk_device()->updateDescriptorSets(desc_writes.size(), desc_writes.data(), 0, nullptr);
	}


	void Gen_mipmap_pass::update(util::Time) {
	}

	void Gen_mipmap_pass::draw(vk::CommandBuffer& command_buffer,
	                   Command_buffer_source&,
	                   vk::DescriptorSet global_uniform_set,
	                   std::size_t) {

		// importance-based mip-map shader produces slidely more artiacts than hardware mip-mapping (and is slower)
		//   => disabled for now
		const auto low_quality_levels = _renderer.gbuffer().mip_levels;//1u;

		// blit the first level
		graphic::generate_mipmaps(command_buffer,
		                          _renderer.gbuffer().depth.image(),
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          _renderer.gbuffer().depth.width(),
		                          _renderer.gbuffer().depth.height(),
		                          low_quality_levels);
		graphic::generate_mipmaps(command_buffer,
		                          _renderer.gbuffer().mat_data.image(),
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          _renderer.gbuffer().mat_data.width(),
		                          _renderer.gbuffer().mat_data.height(),
		                          low_quality_levels);

		// generate mipmaps for GBuffer
		for(auto i=low_quality_levels; i<_renderer.gbuffer().mip_levels; i++) {
			auto& fb = _mipmap_gen_framebuffers.at(i-1);

			// barrier against write to previous mipmap level
			auto subresource = vk::ImageSubresourceRange {
				vk::ImageAspectFlagBits::eColor, gsl::narrow<std::uint32_t>(i-1), 1, 0, 1
			};
			auto barrier = vk::ImageMemoryBarrier {
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead,
				vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				_renderer.gbuffer().depth.image(), subresource
			};
			command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                   vk::PipelineStageFlagBits::eFragmentShader,
			                   vk::DependencyFlags{}, {}, {}, {barrier});

			_mipmap_gen_renderpass.execute(command_buffer, fb, [&] {
				auto descriptor_sets = std::array<vk::DescriptorSet, 2> {
					global_uniform_set,
					*_descriptor_set
				};
				_mipmap_gen_renderpass.bind_descriptor_sets(0, descriptor_sets);

				auto pcs = Push_constants{};
				pcs.arguments.r = i;
				pcs.arguments.g = _renderer.gbuffer().mip_levels - 1;

				_mipmap_gen_renderpass.push_constant("pcs"_strid, pcs);

				command_buffer.draw(3, 1, 0, 0);
			});
		}
	}


	auto Gen_mipmap_pass_factory::create_pass(Deferred_renderer& renderer,
	                                  ecs::Entity_manager& entities,
	                                  util::maybe<Meta_system&> meta_system,
	                                  bool& write_first_pp_buffer) -> std::unique_ptr<Pass> {
		return std::make_unique<Gen_mipmap_pass>(renderer);
	}

	auto Gen_mipmap_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
	                                  int current_score) -> int {
		return current_score;
	}

	void Gen_mipmap_pass_factory::configure_device(vk::PhysicalDevice,
	                                       util::maybe<std::uint32_t>,
	                                       graphic::Device_create_info&) {
	}


}
}
