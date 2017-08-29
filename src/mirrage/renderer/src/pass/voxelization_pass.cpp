#include <mirrage/renderer/pass/voxelization_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/texture.hpp>
#include <mirrage/graphic/window.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <glm/gtx/string_cast.hpp>


namespace mirrage::renderer {

	namespace {
		struct Push_constants {
			glm::mat4 model;
		};

		auto build_render_pass(Deferred_renderer&               renderer,
		                       vk::Format                       data_format,
		                       graphic::Render_target_2D_array& data,
		                       graphic::Framebuffer&            out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto data_attachment = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  data_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto pipeline                           = graphic::Pipeline_description{};
			pipeline.input_assembly.topology        = vk::PrimitiveTopology::eTriangleList;
			pipeline.rasterization.depthClampEnable = true;
			pipeline.multisample                    = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending                 = vk::PipelineColorBlendStateCreateInfo{
			        vk::PipelineColorBlendStateCreateFlags{}, true, vk::LogicOp::eXor};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(renderer.model_loader().material_descriptor_set_layout());
			pipeline.vertex<Model_vertex>(0,
			                              false,
			                              0,
			                              &Model_vertex::position,
			                              1,
			                              &Model_vertex::normal,
			                              2,
			                              &Model_vertex::tex_coords);

			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline).color_attachment(data_attachment);


			pass.stage("voxel"_strid)
			        .shader("frag_shader:voxelization"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:voxelization"_aid, graphic::Shader_stage::vertex);

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
			        {data.view(0), util::Rgba{0, 0, 0, 0}}, data.width(), data.height(), data.layers());

			return render_pass;
		}

		auto get_voxel_format(graphic::Device& device) {
			auto format = device.get_supported_format(
			        {vk::Format::eR32G32B32A32Uint},
			        vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage);

			return format.get_or_throw("No RGB32UInt format support on the device!");
		}
	}


	Voxelization_pass::Voxelization_pass(Deferred_renderer& renderer, ecs::Entity_manager& entities)
	  : _renderer(renderer)
	  , _data_format(get_voxel_format(renderer.device()))
	  , _voxel_data(renderer.device(),
	                {renderer.gbuffer().colorA.width() / 2u, renderer.gbuffer().colorA.height() / 2u, 2u},
	                renderer.gbuffer().mip_levels - 1,
	                _data_format,
	                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	                vk::ImageAspectFlagBits::eColor)
	  , _sampler(renderer.device().create_sampler(12,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eLinear,
	                                              vk::SamplerMipmapMode::eLinear))
	  , _render_pass(build_render_pass(renderer, _data_format, _voxel_data, _framebuffer))
	  , _shadowcasters(entities.list<Shadowcaster_comp>()) {

		INVARIANT(renderer.gbuffer().voxels.is_nothing(),
		          "More than one voxelization implementation active!");
		renderer.gbuffer().voxels = _voxel_data;
	}


	void Voxelization_pass::update(util::Time dt) {}

	void Voxelization_pass::draw(vk::CommandBuffer& command_buffer,
	                             Command_buffer_source&,
	                             vk::DescriptorSet global_uniform_set,
	                             std::size_t) {

		_render_pass.execute(command_buffer, _framebuffer, [&] {
			auto descriptor_sets = std::array<vk::DescriptorSet, 1>{global_uniform_set};
			_render_pass.bind_descriptor_sets(0, descriptor_sets);

			for(auto& caster : _shadowcasters) {
				caster.owner().get<Model_comp>().process([&](Model_comp& model) {
					auto& transform = model.owner().get<ecs::components::Transform_comp>().get_or_throw(
					        "Required Transform_comp missing");

					Push_constants pcs;
					pcs.model = transform.to_mat4();
					_render_pass.push_constant("pcs"_strid, pcs);

					model.model()->bind(command_buffer, _render_pass, 0, [&](auto&, auto offset, auto count) {
						command_buffer.drawIndexed(count, 1, offset, 0, 0);
					});
				});
			}
		});
	}


	auto Voxelization_pass_factory::create_pass(Deferred_renderer&        renderer,
	                                            ecs::Entity_manager&      entities,
	                                            util::maybe<Meta_system&> meta_system,
	                                            bool& write_first_pp_buffer) -> std::unique_ptr<Pass> {
		return std::make_unique<Voxelization_pass>(renderer, entities);
	}

	auto Voxelization_pass_factory::rank_device(vk::PhysicalDevice,
	                                            util::maybe<std::uint32_t> graphics_queue,
	                                            int                        current_score) -> int {
		return current_score;
	}

	void Voxelization_pass_factory::configure_device(vk::PhysicalDevice,
	                                                 util::maybe<std::uint32_t>,
	                                                 graphic::Device_create_info&) {}
}
