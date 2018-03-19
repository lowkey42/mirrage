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

	using namespace graphic;

	namespace {
		struct Push_constants {
			glm::mat4 model;
		};

		constexpr auto texture_count = 2u;

		auto build_render_pass(Deferred_renderer&               renderer,
		                       vk::Format                       data_format,
		                       vk::DescriptorSetLayout          mask_desc_layout,
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
			pipeline.add_descriptor_set_layout(mask_desc_layout);
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

		auto create_bit_mask(std::uint32_t count) -> glm::u32vec4 {
			constexpr auto comp_bits = std::uint32_t(32);

			auto mask = glm::u32vec4(~std::uint32_t(0));

			for(auto i : util::range(count)) {
				mask[i / comp_bits] &= ~(std::uint32_t(1) << (i % comp_bits));
			}
			return mask; // TODO: check
		}

		auto create_mask_texture(Deferred_renderer& renderer, vk::Format data_format) {
			constexpr auto width = 32u * 4u;

			auto write_data = [](char* addr) {
				auto comp_addr = reinterpret_cast<glm::u32vec4*>(addr);
				for(auto i : util::range(width)) {
					*comp_addr = create_bit_mask(i);
					comp_addr++;
				}
			};

			return create_texture<Image_type::single_1d>(renderer.device(),
			                                             {width},
			                                             4 * sizeof(std::uint32_t),
			                                             data_format,
			                                             renderer.queue_family(),
			                                             write_data);
		}
	} // namespace


	Voxelization_pass::Voxelization_pass(Deferred_renderer& renderer, ecs::Entity_manager& entities)
	  : _renderer(renderer)
	  , _sampler(renderer.device().create_sampler(1,
	                                              vk::SamplerAddressMode::eClampToEdge,
	                                              vk::BorderColor::eIntOpaqueBlack,
	                                              vk::Filter::eNearest,
	                                              vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(renderer.device(), *_sampler, 1)
	  , _data_format(get_voxel_format(renderer.device()))
	  , _voxel_data(renderer.device(),
	                {renderer.gbuffer().colorA.width() / 2u,
	                 renderer.gbuffer().colorA.height() / 2u,
	                 texture_count},
	                renderer.gbuffer().mip_levels - 1,
	                _data_format,
	                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	                vk::ImageAspectFlagBits::eColor)
	  , _render_pass(build_render_pass(
	            renderer, _data_format, _descriptor_set_layout.layout(), _voxel_data, _framebuffer))
	  , _models(entities.list<Model_comp>())
	  , _mask_texture(create_mask_texture(renderer, _data_format)) {

		MIRRAGE_INVARIANT(renderer.gbuffer().voxels.is_nothing(),
		                  "More than one voxelization implementation active!");
		renderer.gbuffer().voxels = _voxel_data;
	}


	void Voxelization_pass::update(util::Time) {}

	void Voxelization_pass::draw(vk::CommandBuffer& command_buffer,
	                             Command_buffer_source&,
	                             vk::DescriptorSet global_uniform_set,
	                             std::size_t) {

		if(!_descriptor_set) {
			if(_mask_texture.ready()) {
				_descriptor_set = _descriptor_set_layout.create_set(_renderer.descriptor_pool(),
				                                                    {_mask_texture->view()});

			} else {
				// TODO: clear voxel data?
				return;
			}
		}

		_render_pass.execute(command_buffer, _framebuffer, [&] {
			auto descriptor_sets =
			        std::array<vk::DescriptorSet, 2>{{global_uniform_set, _descriptor_set.get()}};
			_render_pass.bind_descriptor_sets(0, descriptor_sets);

			for(auto& model : _models) {
				auto& transform = model.owner().get<ecs::components::Transform_comp>().get_or_throw(
				        "Required Transform_comp missing");

				Push_constants pcs;
				pcs.model = transform.to_mat4();
				_render_pass.push_constant("pcs"_strid, pcs);

				model.model()->bind_untextured(command_buffer, 0, [&](auto offset, auto count) {
					command_buffer.drawIndexed(count, 1, offset, 0, 0);
				});
			}
		});
	}


	auto Voxelization_pass_factory::create_pass(Deferred_renderer&   renderer,
	                                            ecs::Entity_manager& entities,
	                                            util::maybe<Meta_system&>,
	                                            bool&) -> std::unique_ptr<Pass> {
		return std::make_unique<Voxelization_pass>(renderer, entities);
	}

	auto Voxelization_pass_factory::rank_device(vk::PhysicalDevice,
	                                            util::maybe<std::uint32_t>,
	                                            int current_score) -> int {
		return current_score;
	}

	void Voxelization_pass_factory::configure_device(vk::PhysicalDevice gpu,
	                                                 util::maybe<std::uint32_t>,
	                                                 graphic::Device_create_info& create_info) {

		auto supported_features = gpu.getFeatures();
		MIRRAGE_INVARIANT(supported_features.logicOp, "LogicOp feature is not supported by device!");
		MIRRAGE_INVARIANT(supported_features.depthClamp, "depthClamp feature is not supported by device!");
		MIRRAGE_INVARIANT(supported_features.depthClamp,
		                  "geometryShader feature is not supported by device!");

		create_info.features.logicOp        = true;
		create_info.features.depthClamp     = true;
		create_info.features.geometryShader = true;
	}
} // namespace mirrage::renderer
