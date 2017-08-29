#include <mirrage/renderer/pass/gui_pass.hpp>


namespace mirrage::renderer {

	namespace {
		auto build_render_pass(Deferred_renderer&                 renderer,
		                       vk::DescriptorSetLayout            desc_set_layout,
		                       std::vector<graphic::Framebuffer>& out_framebuffers) {

			auto builder = renderer.device().create_render_pass_builder();

			auto screen =
			        builder.add_attachment(vk::AttachmentDescription{vk::AttachmentDescriptionFlags{},
			                                                         renderer.swapchain().image_format(),
			                                                         vk::SampleCountFlagBits::e1,
			                                                         vk::AttachmentLoadOp::eLoad,
			                                                         vk::AttachmentStoreOp::eStore,
			                                                         vk::AttachmentLoadOp::eDontCare,
			                                                         vk::AttachmentStoreOp::eDontCare,
			                                                         vk::ImageLayout::ePresentSrcKHR,
			                                                         vk::ImageLayout::ePresentSrcKHR});

			auto pipeline                    = graphic::Pipeline_description{};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample             = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending          = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil           = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(desc_set_layout);

			pipeline.add_push_constant("camera"_strid, sizeof(glm::mat4), vk::ShaderStageFlagBits::eVertex);

			pipeline.vertex<gui::Gui_vertex>(0,
			                                 false,
			                                 0,
			                                 &gui::Gui_vertex::position,
			                                 1,
			                                 &gui::Gui_vertex::uv,
			                                 2,
			                                 &gui::Gui_vertex::color);

			auto& pass = builder.add_subpass(pipeline).color_attachment(
			        screen, graphic::all_color_components, graphic::blend_premultiplied_alpha);

			pass.stage("default"_strid)
			        .shader("frag_shader:ui"_aid, graphic::Shader_stage::fragment)
			        .shader("vert_shader:ui"_aid, graphic::Shader_stage::vertex);

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

			for(auto& sc_image : renderer.swapchain().get_images()) {
				out_framebuffers.emplace_back(builder.build_framebuffer({*sc_image, util::Rgba{}},
				                                                        renderer.swapchain().image_width(),
				                                                        renderer.swapchain().image_height()));
			}

			return render_pass;
		}

		auto create_descriptor_set_layout(graphic::Device& device, vk::Sampler sampler)
		        -> vk::UniqueDescriptorSetLayout {
			auto binding = vk::DescriptorSetLayoutBinding{0,
			                                              vk::DescriptorType::eCombinedImageSampler,
			                                              1,
			                                              vk::ShaderStageFlagBits::eFragment,
			                                              &sampler};

			return device.create_descriptor_set_layout(binding);
		}

		constexpr auto max_render_buffer_size = vk::DeviceSize(1024) * 1024;
	}


	Gui_pass::Gui_pass(Deferred_renderer& drenderer, ecs::Entity_manager&, util::maybe<Meta_system&>)
	  : _renderer(drenderer)
	  , _sampler(drenderer.device().create_sampler(1,
	                                               vk::SamplerAddressMode::eClampToEdge,
	                                               vk::BorderColor::eIntOpaqueBlack,
	                                               vk::Filter::eLinear,
	                                               vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(create_descriptor_set_layout(drenderer.device(), *_sampler))
	  , _render_pass(build_render_pass(drenderer, *_descriptor_set_layout, _framebuffers))
	  , _descriptor_set(drenderer.create_descriptor_set(*_descriptor_set_layout))
	  , _mesh_buffer(drenderer.device(),
	                 max_render_buffer_size,
	                 vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer)
	  , _texture_cache(drenderer.device(), drenderer.queue_family()) {}


	void Gui_pass::update(util::Time dt) {}

	void Gui_pass::draw(vk::CommandBuffer& command_buffer,
	                    Command_buffer_source&,
	                    vk::DescriptorSet,
	                    std::size_t swapchain_image) {

		INVARIANT(_current_command_buffer.is_nothing(), "Gui_pass::draw calls cannot be nested!");

		_current_command_buffer = command_buffer;
		_current_framebuffer    = _framebuffers.at(swapchain_image);
		_bound_texture_handle   = -1;

		draw_gui();

		_current_command_buffer = util::nothing;

		// remove unused textures from cache
		for(auto& texture : _loaded_textures) {
			if(texture.use_count() == 1) {
				_renderer.device().destroy_after_frame(std::move(texture->descriptor_set));
				_renderer.device().destroy_after_frame(std::move(texture->texture));
				texture.reset();
			}
		}
		util::erase_if(_loaded_textures, [](auto& t) { return !t; });
		util::erase_if(_loaded_textures_by_aid, [](auto& t) { return t.second.expired(); });
		util::erase_if(_loaded_textures_by_handle, [](auto& t) { return t.second.expired(); });
		_texture_cache.shrink_to_fit();
	}

	Gui_pass::Loaded_texture::Loaded_texture(int                     handle,
	                                         graphic::Texture_ptr    texture,
	                                         vk::Sampler             sampler,
	                                         Deferred_renderer&      renderer,
	                                         vk::DescriptorSetLayout desc_layout)
	  : descriptor_set(renderer.create_descriptor_set(desc_layout))
	  , texture(std::move(texture))
	  , handle{nk_image_id(handle)} {

		auto desc_image = vk::DescriptorImageInfo{
		        sampler, this->texture->view(), vk::ImageLayout::eShaderReadOnlyOptimal};

		auto desc_write = vk::WriteDescriptorSet{
		        *descriptor_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &desc_image, nullptr};

		renderer.device().vk_device()->updateDescriptorSets(1, &desc_write, 0, nullptr);
	}

	auto Gui_pass::load_texture(int width, int height, int channels, const std::uint8_t* data)
	        -> std::shared_ptr<struct nk_image> {
		auto handle = _next_texture_handle++;

		auto dimensions = graphic::Image_dimensions_t<graphic::Image_type::single_2d>{
		        gsl::narrow<std::uint32_t>(width), gsl::narrow<std::uint32_t>(height)};

		auto texture = std::make_shared<graphic::Texture_2D>(
		        _renderer.device(),
		        dimensions,
		        false,
		        gsl::narrow<std::uint32_t>(channels),
		        true,
		        gsl::span<const std::uint8_t>{data, width * height * channels},
		        _renderer.queue_family());

		auto entry = std::make_shared<Loaded_texture>(
		        handle, std::move(texture), *_sampler, _renderer, *_descriptor_set_layout);

		_loaded_textures_by_handle.emplace(handle, entry);

		auto return_value = std::shared_ptr<struct nk_image>(entry, &entry->handle);

		_loaded_textures.emplace_back(std::move(entry));

		return return_value;
	}

	auto Gui_pass::load_texture(const asset::AID& aid) -> std::shared_ptr<struct nk_image> {
		auto cache_entry = _loaded_textures_by_aid[aid];
		if(auto sp = cache_entry.lock()) {
			return sp;
		}

		auto handle = _next_texture_handle++;

		auto entry = std::make_shared<Loaded_texture>(
		        handle, _texture_cache.load(aid), *_sampler, _renderer, *_descriptor_set_layout);

		_loaded_textures_by_handle.emplace(handle, entry);

		auto return_value = std::shared_ptr<struct nk_image>(entry, &entry->handle);

		_loaded_textures.emplace_back(std::move(entry));

		cache_entry = return_value;
		return return_value;
	}


	void Gui_pass::prepare_draw(gsl::span<const std::uint16_t>   indices,
	                            gsl::span<const gui::Gui_vertex> vertices,
	                            glm::mat4                        view_proj) {

		auto& cb = _current_command_buffer.get_or_throw(
		        "Gui_pass::prepare_draw ha to be "
		        "called inside a draw call!");
		auto& fb = _current_framebuffer.get_or_throw(
		        "Gui_pass::prepare_draw ha to be "
		        "called inside a draw call!");

		auto index_offset = vertices.size_bytes();
		_mesh_buffer.update_objs(0, graphic::to_bytes(vertices));
		_mesh_buffer.update_objs(index_offset, graphic::to_bytes(indices));
		_mesh_buffer.flush(cb,
		                   vk::PipelineStageFlagBits::eVertexInput,
		                   vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eIndexRead);


		_render_pass.unsafe_begin_renderpass(cb, fb); // ended by finalize_draw()


		_render_pass.push_constant("camera"_strid, view_proj);

		cb.bindVertexBuffers(0, {_mesh_buffer.buffer()}, {0});
		cb.bindIndexBuffer(_mesh_buffer.buffer(), index_offset, vk::IndexType::eUint16);
	}

	void Gui_pass::draw_elements(int           texture_handle,
	                             glm::vec4     clip_rect,
	                             std::uint32_t offset,
	                             std::uint32_t count) {
		auto& cb = _current_command_buffer.get_or_throw(
		        "Gui_pass::prepare_draw ha to be "
		        "called inside a draw call!");

		if(texture_handle != _bound_texture_handle) {
			auto texture = _loaded_textures_by_handle[texture_handle].lock();
			INVARIANT(texture, "The requested texture has not been loaded or already been freed!");

			_render_pass.bind_descriptor_sets(0, {&*texture->descriptor_set, 1});
			_bound_texture_handle = texture_handle;
		}

		cb.setScissor(
		        0,
		        {vk::Rect2D{vk::Offset2D(clip_rect.x, clip_rect.y), vk::Extent2D(clip_rect.z, clip_rect.w)}});

		cb.drawIndexed(count, 1, offset, 0, 0);
	}

	void Gui_pass::finalize_draw() { _render_pass.unsafe_end_renderpass(); }


	auto Gui_pass_factory::create_pass(Deferred_renderer&        renderer,
	                                   ecs::Entity_manager&      entities,
	                                   util::maybe<Meta_system&> meta_system,
	                                   bool&) -> std::unique_ptr<Pass> {
		return std::make_unique<Gui_pass>(renderer, entities, meta_system);
	}

	auto Gui_pass_factory::rank_device(vk::PhysicalDevice,
	                                   util::maybe<std::uint32_t> graphics_queue,
	                                   int                        current_score) -> int {
		return current_score;
	}

	void Gui_pass_factory::configure_device(vk::PhysicalDevice,
	                                        util::maybe<std::uint32_t>,
	                                        graphic::Device_create_info&) {}
}
