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
			                                  vk::ImageLayout::eDepthStencilAttachmentOptimal});

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
			pipeline.rasterization.cullMode = vk::CullModeFlagBits::eFront;
			pipeline.depth_stencil          = vk::PipelineDepthStencilStateCreateInfo{
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

	Shadowmap::Shadowmap(Deferred_renderer& r, graphic::Device& device, std::int32_t size, vk::Format format)
	  : texture(device,
	            {size, size},
	            1,
	            format,
	            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	                    | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
	            vk::ImageAspectFlagBits::eColor)
	  , model_group(r.reserve_secondary_command_buffer_group())
	  , model_animated_group(r.reserve_secondary_command_buffer_group())
	  , model_animated_dqs_group(r.reserve_secondary_command_buffer_group())
	{
	}
	Shadowmap::Shadowmap(Shadowmap&& rhs) noexcept
	  : texture(std::move(rhs.texture))
	  , framebuffer(std::move(rhs.framebuffer))
	  , owner(std::move(rhs.owner))
	  , light_source_position(std::move(rhs.light_source_position))
	  , light_source_orientation(std::move(rhs.light_source_orientation))
	  , culling_mask(rhs.culling_mask)
	  , model_group(rhs.model_group)
	  , model_animated_group(rhs.model_animated_group)
	  , model_animated_dqs_group(rhs.model_animated_dqs_group)
	  , view_proj(rhs.view_proj)
	{
	}

	Shadowmapping_pass::Shadowmapping_pass(Deferred_renderer& renderer, ecs::Entity_manager& entities)
	  : Render_pass(renderer)
	  , _entities(entities)
	  , _shadowmap_format(get_shadowmap_format(renderer.device()))
	  , _depth(renderer.device(),
	           {renderer.settings().shadowmap_resolution, renderer.settings().shadowmap_resolution},
	           1,
	           get_depth_format(renderer.device()),
	           vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
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
		                                   return Shadowmap(renderer,
		                                                    renderer.device(),
		                                                    renderer.settings().shadowmap_resolution,
		                                                    _shadowmap_format);
	                                   }))
	  , _render_pass(build_render_pass(
	            renderer, _depth, get_depth_format(renderer.device()), _shadowmap_format, _shadowmaps))
	  , _model_stage(_render_pass.get_stage(0, "model"_strid, "pcs"_strid))
	  , _model_animated_stage(_render_pass.get_stage(1, "model"_strid, "pcs"_strid))
	  , _model_animate_dqs_stage(_render_pass.get_stage(1, "model_dqs"_strid, "pcs"_strid))
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
		        gsl::narrow<uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);
	}

	void Shadowmapping_pass::pre_draw(Frame_data& frame)
	{
		// free shadowmaps of deleted lights
		for(auto& sm : _shadowmaps) {
			if(sm.owner && !_entities.validate(sm.owner)) {
				sm.owner = ecs::invalid_entity;
			}
		}
	}

	namespace {
		enum class Shadowpass_stage { normal, animated, animated_dqs };
	}
	struct Shadowmapping_pass::Shadowmapping_pass_impl_helper {
		template <Shadowpass_stage stage, typename F>
		static void handle_obj(Shadowmapping_pass& self,
		                       Frame_data&         frame,
		                       Culling_mask        mask,
		                       const glm::mat4&    transform,
		                       F&&                 callback)
		{
			auto pcs  = Push_constants{};
			pcs.model = transform;

			const auto& stage_ref =
			        stage == Shadowpass_stage::normal
			                ? self._model_stage
			                : (stage == Shadowpass_stage::animated ? self._model_animated_stage
			                                                       : self._model_animate_dqs_stage);

			for(auto& shadowmap : self._shadowmaps) {
				shadowmap.culling_mask.process([&](auto& shadowmap_mask) {
					if((shadowmap_mask & mask) == shadowmap_mask) {
						const auto group = stage == Shadowpass_stage::normal
						                           ? shadowmap.model_group
						                           : (stage == Shadowpass_stage::animated
						                                      ? shadowmap.model_animated_group
						                                      : shadowmap.model_animated_dqs_group);

						pcs.light_view_proj = shadowmap.view_proj;

						auto [cmd_buffer, empty] = self._renderer.get_secondary_command_buffer(group);
						if(empty) {
							stage_ref.begin(cmd_buffer, shadowmap.framebuffer);
							stage_ref.bind_descriptor_set(cmd_buffer, 0, frame.global_uniform_set);
						}

						stage_ref.push_constant(cmd_buffer, pcs);

						callback(cmd_buffer, stage_ref);
					}
				});
			}
		}
	};

	void Shadowmapping_pass::handle_obj(Frame_data&  frame,
	                                    Culling_mask mask,
	                                    ecs::Entity_facet,
	                                    const glm::vec4&,
	                                    const glm::mat4& transform,
	                                    const Model&     model,
	                                    const Sub_mesh&  sub_mesh)
	{
		Shadowmapping_pass_impl_helper::handle_obj<Shadowpass_stage::normal>(
		        *this, frame, mask, transform, [&](auto&& cmd_buffer, auto&& stage) {
			        stage.bind_descriptor_set(cmd_buffer, 1, sub_mesh.material->desc_set());
			        model.bind_mesh(cmd_buffer, 0);

			        cmd_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		        });
	}
	void Shadowmapping_pass::handle_obj(Frame_data&  frame,
	                                    Culling_mask mask,
	                                    ecs::Entity_facet,
	                                    const glm::vec4&,
	                                    const glm::mat4& transform,
	                                    const Model&     model,
	                                    Skinning_type    skinning_type,
	                                    std::uint32_t    pose_offset)
	{
		auto callback = [&](auto&& cmd_buffer, auto&& stage) {
			auto first = true;
			for(auto& sub_mesh : model.sub_meshes()) {
				stage.bind_descriptor_set(cmd_buffer, 1, sub_mesh.material->desc_set());

				if(first) {
					first        = false;
					auto offsets = std::array<std::uint32_t, 1>{pose_offset};
					stage.bind_descriptor_set(cmd_buffer, 2, _renderer.gbuffer().animation_data, offsets);
					model.bind_mesh(cmd_buffer, 0);
				}

				cmd_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
			}
		};

		if(skinning_type == Skinning_type::dual_quaternion_skinning)
			Shadowmapping_pass_impl_helper::handle_obj<Shadowpass_stage::animated_dqs>(
			        *this, frame, mask, transform, callback);
		else
			Shadowmapping_pass_impl_helper::handle_obj<Shadowpass_stage::animated>(
			        *this, frame, mask, transform, callback);
	}

	void Shadowmapping_pass::post_draw(Frame_data& frame)
	{
		auto _ = _mark_subpass(frame);

		if(_first_frame) {
			_first_frame = false;
			for(auto& sm : _shadowmaps) {
				graphic::clear_texture(frame.main_command_buffer,
				                       sm.texture.image(),
				                       sm.texture.width(),
				                       sm.texture.height(),
				                       util::Rgba{0, 0, 0, 1},
				                       vk::ImageLayout::eUndefined,
				                       vk::ImageLayout::eShaderReadOnlyOptimal,
				                       0,
				                       1);
			}
		}

		// free shadowmaps of deleted lights
		for(auto& sm : _shadowmaps) {
			if(sm.owner && !_entities.validate(sm.owner)) {
				sm.owner = ecs::invalid_entity;
			}
		}

		// update shadow maps
		auto& commands = frame.main_command_buffer;

		for(auto& shadowmap : _shadowmaps) {
			if(shadowmap.culling_mask.is_some()) {
				shadowmap.culling_mask = util::nothing;

				const auto content = vk::SubpassContents::eSecondaryCommandBuffers;
				_render_pass.execute(commands, shadowmap.framebuffer, content, [&] {
					// static models
					_renderer.execute_group(shadowmap.model_group, commands);

					_render_pass.next_subpass(content);

					// animated models (lbs)
					_renderer.execute_group(shadowmap.model_animated_group, commands);

					// animated models (dqs)
					_renderer.execute_group(shadowmap.model_animated_dqs_group, commands);
				});
			}
		}
	}


	auto Shadowmapping_pass_factory::create_pass(Deferred_renderer& renderer,
	                                             std::shared_ptr<void>,
	                                             util::maybe<ecs::Entity_manager&> entities,
	                                             Engine&,
	                                             bool&) -> std::unique_ptr<Render_pass>
	{
		if(renderer.settings().shadows)
			return std::make_unique<Shadowmapping_pass>(
			        renderer, entities.get_or_throw("Shadowmapping_pass requires an entitymanager."));
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
