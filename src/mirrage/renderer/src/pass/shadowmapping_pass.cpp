#include <mirrage/renderer/pass/shadowmapping_pass.hpp>

#include <mirrage/renderer/light_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	using namespace graphic;
	using namespace util::unit_literals;
	using ecs::components::Transform_comp;

	namespace {
		struct Push_constants {
			glm::mat4 model;
			glm::mat4 light_view_proj;
		};

		auto build_render_pass(Deferred_renderer&         renderer,
		                       graphic::Render_target_2D& depth_buffer,
		                       vk::Format                 depth_format,
		                       vk::Format                 shadowmap_format,
		                       std::vector<Shadowmap>&    shadowmaps)
		{
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

			// normal models
			auto model_pipeline = pipeline;
			model_pipeline.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
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

			// rigged models
			auto rigged_model_pipeline = pipeline;
			rigged_model_pipeline.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
			rigged_model_pipeline.add_descriptor_set_layout(*renderer.gbuffer().animation_data_layout);
			rigged_model_pipeline.vertex<Model_rigged_vertex>(0,
			                                                  false,
			                                                  0,
			                                                  &Model_rigged_vertex::position,
			                                                  1,
			                                                  &Model_rigged_vertex::normal,
			                                                  2,
			                                                  &Model_rigged_vertex::tex_coords,
			                                                  3,
			                                                  &Model_rigged_vertex::bone_ids,
			                                                  4,
			                                                  &Model_rigged_vertex::bone_weights);

			auto& rigged_model_pass = builder.add_subpass(rigged_model_pipeline)
			                                  .color_attachment(shadowmap)
			                                  .depth_stencil_attachment(depth);

			rigged_model_pass.stage("model"_strid)
			        .shader("frag_shader:shadow_model"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:shadow_model_animated"_aid, graphic::Shader_stage::vertex);

			rigged_model_pass.stage("model_dqs"_strid)
			        .shader("frag_shader:shadow_model"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:shadow_model_animated_dqs"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(model_pass,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput
			                               | vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eLateFragmentTests,
			                       vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			                       rigged_model_pass,
			                       vk::PipelineStageFlagBits::eEarlyFragmentTests
			                               | vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead
			                               | vk::AccessFlagBits::eColorAttachmentWrite
			                               | vk::AccessFlagBits::eDepthStencilAttachmentRead
			                               | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

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


		auto get_shadowmap_format(graphic::Device& device)
		{
			auto format = device.get_supported_format(
			        {vk::Format::eR32Sfloat, vk::Format::eR16Sfloat, vk::Format::eR32G32B32A32Sfloat},
			        vk::FormatFeatureFlagBits::eColorAttachment
			                | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			MIRRAGE_INVARIANT(format.is_some(), "Shadowmap (R32 / R16) render targets are not supported!");

			return format.get_or_throw();
		}
		auto get_depth_format(graphic::Device& device)
		{
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

	Shadowmap::Shadowmap(graphic::Device& device, std::int32_t size, vk::Format format)
	  : texture(device,
	            {size, size},
	            1,
	            format,
	            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                    | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
	            vk::ImageAspectFlagBits::eColor)
	{
	}
	Shadowmap::Shadowmap(Shadowmap&& rhs) noexcept
	  : texture(std::move(rhs.texture))
	  , framebuffer(std::move(rhs.framebuffer))
	  , owner(std::move(rhs.owner))
	  , light_source_position(std::move(rhs.light_source_position))
	  , light_source_orientation(std::move(rhs.light_source_orientation))
	{
	}

	Shadowmapping_pass::Shadowmapping_pass(Deferred_renderer& renderer, ecs::Entity_manager& entities)
	  : _renderer(renderer)
	  , _entities(entities)
	  , _shadowmap_format(get_shadowmap_format(renderer.device()))
	  , _depth(renderer.device(),
	           {renderer.settings().shadowmap_resolution, renderer.settings().shadowmap_resolution},
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
	  , _shadowmaps(util::build_vector(renderer.gbuffer().max_shadowmaps,
	                                   [&](auto) {
		                                   return Shadowmap(renderer.device(),
		                                                    renderer.settings().shadowmap_resolution,
		                                                    _shadowmap_format);
	                                   }))
	  , _render_pass(build_render_pass(
	            renderer, _depth, get_depth_format(renderer.device()), _shadowmap_format, _shadowmaps))
	{
		entities.register_component_type<Directional_light_comp>();
		entities.register_component_type<Shadowcaster_comp>();

		renderer.gbuffer().shadowmapping_enabled = true;


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

	void Shadowmapping_pass::update(util::Time) {}

	void Shadowmapping_pass::draw(Frame_data& frame)
	{

		// free shadowmaps of deleted lights
		for(auto& sm : _shadowmaps) {
			if(sm.owner && !_entities.validate(sm.owner)) {
				sm.owner = ecs::invalid_entity;
			}
		}

		// update shadow maps
		auto pcs = Push_constants{};


		for(auto& [entity, transform, light_variant, mask] : frame.light_queue) {
			if(mask == 0)
				continue;

			if(auto ll = std::get_if<Directional_light_comp*>(&light_variant); ll) {
				auto& light = **ll;

				if(light.shadowmap_id() == -1) {
					for(auto i = 0u; i < _shadowmaps.size(); i++) {
						if(!_shadowmaps[i].owner) {
							light.shadowmap_id(int(i));
							_shadowmaps[i].owner = light.owner_handle();
							break;
						}
					}
				}

				if(light.shadowmap_id() == -1 || !light.on_update()) {
					continue;
				}

				auto& shadowmap = _shadowmaps.at(gsl::narrow<std::size_t>(light.shadowmap_id()));
				shadowmap.light_source_position    = transform->position;
				shadowmap.light_source_orientation = transform->orientation;
				shadowmap.caster_count             = _entities.list<Model_comp>().size();

				pcs.light_view_proj = light.calc_shadowmap_view_proj(*transform);

				auto& target_fb = shadowmap.framebuffer;
				_render_pass.execute(frame.main_command_buffer, target_fb, [&, mask = mask] {
					_render_pass.bind_descriptor_sets(0, {&frame.global_uniform_set, 1});

					auto waiting_for_rigged = true;
					auto last_material      = static_cast<const Material*>(nullptr);
					auto last_model         = static_cast<const Model*>(nullptr);
					auto last_dqs           = false;

					for(auto& geo : frame.partition_geometry(mask)) {
						if(geo.model->rigged() && waiting_for_rigged) {
							waiting_for_rigged = false;
							last_material      = nullptr;
							last_model         = nullptr;
							_render_pass.next_subpass();
							_render_pass.set_stage("model"_strid);
						}

						// bind mesh and material
						auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
						if(&*sub_mesh.material != last_material) {
							last_material = &*sub_mesh.material;
							last_material->bind(_render_pass);
						}
						if(geo.model != last_model) {
							last_model = geo.model;
							geo.model->bind_mesh(frame.main_command_buffer, 0);
						}

						// bind animation pose
						if(geo.model->rigged()) {
							auto uniform_offset = geo.animation_uniform_offset.get_or_throw();
							_render_pass.bind_descriptor_set(
							        2, _renderer.gbuffer().animation_data, {&uniform_offset, 1u});

							auto dqs = geo.substance_id == "dq_default"_strid
							           || geo.substance_id == "dq_emissive"_strid
							           || geo.substance_id == "dq_alphatest"_strid;

							if(dqs != last_dqs) {
								last_dqs = dqs;
								_render_pass.set_stage(dqs ? "model_dqs"_strid : "model"_strid);
							}
						}

						// set transformation
						pcs.model    = glm::toMat4(geo.orientation) * glm::scale(glm::mat4(1.f), geo.scale);
						pcs.model[3] = glm::vec4(geo.position, 1.f);
						_render_pass.push_constant("pcs"_strid, pcs);

						// draw
						frame.main_command_buffer.drawIndexed(
						        sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
					}

					if(waiting_for_rigged)
						_render_pass.next_subpass();
				});
			}
		}
	}


	auto Shadowmapping_pass_factory::create_pass(Deferred_renderer&   renderer,
	                                             ecs::Entity_manager& entities,
	                                             Engine&,
	                                             bool&) -> std::unique_ptr<Render_pass>
	{
		if(renderer.settings().shadows)
			return std::make_unique<Shadowmapping_pass>(renderer, entities);
		else
			return {};
	}

	auto Shadowmapping_pass_factory::rank_device(vk::PhysicalDevice,
	                                             util::maybe<std::uint32_t>,
	                                             int current_score) -> int
	{
		return current_score;
	}

	void Shadowmapping_pass_factory::configure_device(vk::PhysicalDevice,
	                                                  util::maybe<std::uint32_t>,
	                                                  graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
