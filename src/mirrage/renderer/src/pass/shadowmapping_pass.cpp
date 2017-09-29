#include <mirrage/renderer/pass/shadowmapping_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/renderer/model_comp.hpp>


namespace mirrage::renderer {

	using namespace graphic;
	using namespace util::unit_literals;

	namespace {
		struct Push_constants {
			glm::mat4 model;
			glm::mat4 light_view_proj;
		};

		auto build_render_pass(Deferred_renderer&         renderer,
		                       graphic::Render_target_2D& depth_buffer,
		                       vk::Format                 depth_format,
		                       vk::Format                 shadowmap_format,
		                       std::vector<Shadowmap>&    shadowmaps) {
			auto builder = renderer.device().create_render_pass_builder();

			auto depth = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  depth_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eClear,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});

			auto shadowmap = builder.add_attachment(
			        vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                  shadowmap_format,
			                                  vk::SampleCountFlagBits::e1,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eStore,
			                                  vk::AttachmentLoadOp::eDontCare,
			                                  vk::AttachmentStoreOp::eDontCare,
			                                  vk::ImageLayout::eUndefined,
			                                  vk::ImageLayout::eShaderReadOnlyOptimal});


			auto pipeline                    = Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			// Back/Frontface-Culling is disabled (for now) because the test scene contains one-sided
			//   geometry and the TAA clears up self-shadowing-artefacts anyway.
			// pipeline.rasterization.cullMode = vk::CullModeFlagBits::eBack;
			pipeline.multisample    = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil  = vk::PipelineDepthStencilStateCreateInfo{
                    vk::PipelineDepthStencilStateCreateFlags{}, true, true, vk::CompareOp::eLess};
			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());

			pipeline.add_push_constant("pcs"_strid,
			                           sizeof(Push_constants),
			                           vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);


			auto model_pipeline = pipeline;
			model_pipeline.add_descriptor_set_layout(
			        renderer.model_loader().material_descriptor_set_layout());
			model_pipeline.vertex<Model_vertex>(0,
			                                    false,
			                                    0,
			                                    &Model_vertex::position,
			                                    1,
			                                    &Model_vertex::normal,
			                                    2,
			                                    &Model_vertex::tex_coords);

			auto& model_pass = builder.add_subpass(model_pipeline)
			                           .color_attachment(shadowmap)
			                           .depth_stencil_attachment(depth);

			model_pass.stage("model"_strid)
			        .shader("frag_shader:shadow_model"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:shadow_model"_aid, graphic::Shader_stage::vertex);


			builder.add_dependency(
			        util::nothing,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlags{},
			        model_pass,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

			builder.add_dependency(
			        model_pass,
			        vk::PipelineStageFlagBits::eColorAttachmentOutput,
			        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
			        util::nothing,
			        vk::PipelineStageFlagBits::eBottomOfPipe,
			        vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eShaderRead
			                | vk::AccessFlagBits::eTransferRead);

			auto render_pass = builder.build();


			for(auto& sm : shadowmaps) {
				auto sm_attachments = std::array<Framebuffer_attachment_desc, 2>{
				        {{depth_buffer.view(0), 1.f}, {sm.texture.view(0), util::Rgba(1.f)}}};
				sm.framebuffer = builder.build_framebuffer(sm_attachments,
				                                           renderer.settings().shadowmap_resolution,
				                                           renderer.settings().shadowmap_resolution);
			}

			return render_pass;
		}


		auto get_shadowmap_format(graphic::Device& device) {
			auto format = device.get_supported_format(
			        {vk::Format::eR32Sfloat, vk::Format::eR16Sfloat, vk::Format::eR32G32B32A32Sfloat},
			        vk::FormatFeatureFlagBits::eColorAttachment
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			MIRRAGE_INVARIANT(format.is_some(), "Shadowmap (R32 / R16) render targets are not supported!");

			return format.get_or_throw();
		}
		auto get_depth_format(graphic::Device& device) {
			auto format = device.get_supported_format({vk::Format::eD16Unorm,
			                                           vk::Format::eD16UnormS8Uint,
			                                           vk::Format::eD24UnormS8Uint,
			                                           vk::Format::eD32Sfloat,
			                                           vk::Format::eD32SfloatS8Uint},
			                                          vk::FormatFeatureFlagBits::eDepthStencilAttachment);

			MIRRAGE_INVARIANT(format.is_some(), "Depth render targets are not supported!");

			return format.get_or_throw();
		}
	} // namespace

	Shadowmap::Shadowmap(graphic::Device& device, std::uint32_t size, vk::Format format)
	  : texture(device,
	            {size, size},
	            1,
	            format,
	            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                    | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
	            vk::ImageAspectFlagBits::eColor) {}
	Shadowmap::Shadowmap(Shadowmap&& rhs) noexcept
	  : texture(std::move(rhs.texture))
	  , framebuffer(std::move(rhs.framebuffer))
	  , owner(std::move(rhs.owner))
	  , light_source_position(std::move(rhs.light_source_position))
	  , light_source_orientation(std::move(rhs.light_source_orientation)) {}

	Shadowmapping_pass::Shadowmapping_pass(Deferred_renderer&   renderer,
	                                       ecs::Entity_manager& entities,
	                                       util::maybe<Meta_system&>)
	  : _renderer(renderer)
	  , _entities(entities)
	  , _shadowmap_format(get_shadowmap_format(renderer.device()))
	  , _depth(renderer.device(),
	           {gsl::narrow<std::uint32_t>(renderer.settings().shadowmap_resolution),
	            gsl::narrow<std::uint32_t>(renderer.settings().shadowmap_resolution)},
	           1,
	           get_depth_format(renderer.device()),
	           vk::ImageUsageFlagBits::eDepthStencilAttachment,
	           vk::ImageAspectFlagBits::eDepth)
	  , _shadowmap_sampler(renderer.device().create_sampler(1,
	                                                        vk::SamplerAddressMode::eClampToBorder,
	                                                        vk::BorderColor::eFloatOpaqueWhite,
	                                                        vk::Filter::eLinear,
	                                                        vk::SamplerMipmapMode::eNearest,
	                                                        false,
	                                                        vk::CompareOp::eLess))
	  , _shadowmap_depth_sampler(renderer.device().create_sampler(1,
	                                                              vk::SamplerAddressMode::eClampToBorder,
	                                                              vk::BorderColor::eFloatOpaqueWhite,
	                                                              vk::Filter::eLinear,
	                                                              vk::SamplerMipmapMode::eNearest,
	                                                              false))
	  , _lights_directional(entities.list<Directional_light_comp>())
	  , _shadowcasters(entities.list<Shadowcaster_comp>())
	  , _shadowmaps(util::make_vector(
	            Shadowmap(renderer.device(), renderer.settings().shadowmap_resolution, _shadowmap_format),
	            Shadowmap(renderer.device(), renderer.settings().shadowmap_resolution, _shadowmap_format)))
	  , _render_pass(build_render_pass(
	            renderer, _depth, get_depth_format(renderer.device()), _shadowmap_format, _shadowmaps)) {

		entities.register_component_type<Shadowcaster_comp>();

		auto shadowmap_bindings = std::array<vk::DescriptorSetLayoutBinding, 3>();
		shadowmap_bindings[0]   = vk::DescriptorSetLayoutBinding{0,
                                                               vk::DescriptorType::eSampledImage,
                                                               gsl::narrow<std::uint32_t>(_shadowmaps.size()),
                                                               vk::ShaderStageFlagBits::eFragment};
		shadowmap_bindings[1]   = vk::DescriptorSetLayoutBinding{
                1, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment};
		shadowmap_bindings[2] = vk::DescriptorSetLayoutBinding{
		        2, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment};
		MIRRAGE_INVARIANT(!renderer.gbuffer().shadowmaps_layout,
		                  "More than one shadowmapping implementation active!");
		renderer.gbuffer().shadowmaps_layout =
		        renderer.device().create_descriptor_set_layout(shadowmap_bindings);
		renderer.gbuffer().shadowmaps = renderer.create_descriptor_set(*renderer.gbuffer().shadowmaps_layout);


		auto shadowmap_infos = std::vector<vk::DescriptorImageInfo>();
		shadowmap_infos.reserve(_shadowmaps.size());
		for(auto& sm : _shadowmaps) {
			shadowmap_infos.emplace_back(
			        *_shadowmap_sampler, sm.texture.view(), vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		auto desc_writes = std::array<vk::WriteDescriptorSet, 3>();
		desc_writes[0]   = vk::WriteDescriptorSet{*renderer.gbuffer().shadowmaps,
                                                0,
                                                0,
                                                gsl::narrow<std::uint32_t>(shadowmap_infos.size()),
                                                vk::DescriptorType::eSampledImage,
                                                shadowmap_infos.data()};

		auto shadow_sampler = vk::DescriptorImageInfo{*_shadowmap_sampler};
		desc_writes[1]      = vk::WriteDescriptorSet{
                *renderer.gbuffer().shadowmaps, 1, 0, 1, vk::DescriptorType::eSampler, &shadow_sampler};
		auto depth_sampler = vk::DescriptorImageInfo{*_shadowmap_depth_sampler};
		desc_writes[2]     = vk::WriteDescriptorSet{
                *renderer.gbuffer().shadowmaps, 2, 0, 1, vk::DescriptorType::eSampler, &depth_sampler};

		renderer.device().vk_device()->updateDescriptorSets(
		        desc_writes.size(), desc_writes.data(), 0, nullptr);
	}

	void Shadowmapping_pass::update(util::Time dt) {}

	void Shadowmapping_pass::draw(vk::CommandBuffer& command_buffer,
	                              Command_buffer_source&,
	                              vk::DescriptorSet global_uniform_set,
	                              std::size_t) {

		// free shadowmaps of deleted lights
		for(auto& sm : _shadowmaps) {
			if(sm.owner && !_entities.validate(sm.owner)) {
				sm.owner = ecs::invalid_entity;
			}
		}

		// update shadow maps
		auto pcs = Push_constants{};

		for(auto& light : _lights_directional) {
			if(!light.owner().has<Shadowcaster_comp>())
				continue;

			auto& light_transform = light.owner().get<ecs::components::Transform_comp>().get_or_throw();

			if(light.shadowmap_id() == -1) {
				for(auto i = 0; i < gsl::narrow<int>(_shadowmaps.size()); i++) {
					if(!_shadowmaps[i].owner) {
						light.shadowmap_id(i);
						_shadowmaps[i].owner = light.owner_handle();
						break;
					}
				}

				if(light.shadowmap_id() == -1) {
					continue;
				}

			} else if(!_renderer.settings().dynamic_shadows) {
				auto& shadowmap = _shadowmaps.at(light.shadowmap_id());

				auto pos_diff = glm::length2(light_transform.position() - shadowmap.light_source_position);
				auto orientation_diff = glm::abs(
				        glm::dot(light_transform.orientation(), shadowmap.light_source_orientation) - 1);
				if(pos_diff <= 0.0001f && orientation_diff <= 0.001f
				   && shadowmap.caster_count == _entities.list<Model_comp>().size()) {
					continue; // skip update
				}
			}

			auto& shadowmap                    = _shadowmaps.at(light.shadowmap_id());
			shadowmap.light_source_position    = light_transform.position();
			shadowmap.light_source_orientation = light_transform.orientation();
			shadowmap.caster_count             = _entities.list<Model_comp>().size();

			pcs.light_view_proj = light.calc_shadowmap_view_proj();

			auto& target_fb = shadowmap.framebuffer;
			_render_pass.execute(command_buffer, target_fb, [&] {
				_render_pass.bind_descriptor_sets(0, {&global_uniform_set, 1});

				for(auto& caster : _shadowcasters) {
					caster.owner().get<Model_comp>().process([&](Model_comp& model) {
						auto& transform = model.owner().get<ecs::components::Transform_comp>().get_or_throw(
						        "Required Transform_comp missing");

						pcs.model = transform.to_mat4();
						_render_pass.push_constant("pcs"_strid, pcs);

						model.model()->bind(
						        command_buffer, _render_pass, 0, [&](auto&, auto offset, auto count) {
							        command_buffer.drawIndexed(count, 1, offset, 0, 0);
						        });
					});
				}
			});
		}
	}


	auto Shadowmapping_pass_factory::create_pass(Deferred_renderer&        renderer,
	                                             ecs::Entity_manager&      entities,
	                                             util::maybe<Meta_system&> meta_system,
	                                             bool&) -> std::unique_ptr<Pass> {
		return std::make_unique<Shadowmapping_pass>(renderer, entities, meta_system);
	}

	auto Shadowmapping_pass_factory::rank_device(vk::PhysicalDevice,
	                                             util::maybe<std::uint32_t> graphics_queue,
	                                             int                        current_score) -> int {
		return current_score;
	}

	void Shadowmapping_pass_factory::configure_device(vk::PhysicalDevice,
	                                                  util::maybe<std::uint32_t>,
	                                                  graphic::Device_create_info&) {}
} // namespace mirrage::renderer
