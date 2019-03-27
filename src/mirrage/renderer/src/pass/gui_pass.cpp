#include <mirrage/renderer/pass/gui_pass.hpp>

#include <mirrage/engine.hpp>


namespace mirrage::renderer {

	namespace {
		auto build_render_pass(Deferred_renderer&                 renderer,
		                       vk::DescriptorSetLayout            desc_set_layout,
		                       std::vector<graphic::Framebuffer>& out_framebuffers)
		{

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

			auto render_pass = builder.build();

			for(auto& sc_image : renderer.swapchain().get_image_views()) {
				out_framebuffers.emplace_back(builder.build_framebuffer({*sc_image, util::Rgba{}},
				                                                        renderer.swapchain().image_width(),
				                                                        renderer.swapchain().image_height()));
			}

			return render_pass;
		}

		auto create_descriptor_set_layout(graphic::Device& device, vk::Sampler sampler)
		        -> vk::UniqueDescriptorSetLayout
		{
			auto binding = vk::DescriptorSetLayoutBinding{0,
			                                              vk::DescriptorType::eCombinedImageSampler,
			                                              1,
			                                              vk::ShaderStageFlagBits::eFragment,
			                                              &sampler};

			return device.create_descriptor_set_layout(binding);
		}

		constexpr auto max_render_buffer_size = vk::DeviceSize(1024) * 1024 * 10;
	} // namespace


	Gui_pass::Gui_pass(Deferred_renderer& drenderer, Engine&)
	  : _renderer(drenderer)
	  , _sampler(drenderer.device().create_sampler(1,
	                                               vk::SamplerAddressMode::eClampToEdge,
	                                               vk::BorderColor::eIntOpaqueBlack,
	                                               vk::Filter::eLinear,
	                                               vk::SamplerMipmapMode::eNearest))
	  , _descriptor_set_layout(create_descriptor_set_layout(drenderer.device(), *_sampler))
	  , _render_pass(build_render_pass(drenderer, *_descriptor_set_layout, _framebuffers))
	  , _descriptor_set(drenderer.create_descriptor_set(*_descriptor_set_layout, 1))
	  , _mesh_buffer(drenderer.device(),
	                 max_render_buffer_size,
	                 vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer)
	{
	}


	void Gui_pass::update(util::Time) {}

	void Gui_pass::draw(Frame_data& frame)
	{
		MIRRAGE_INVARIANT(_current_command_buffer.is_nothing(), "Gui_pass::draw calls cannot be nested!");

		_current_command_buffer = frame.main_command_buffer;
		_current_framebuffer    = _framebuffers.at(frame.swapchain_image);
		_bound_texture_handle   = util::nothing;

		draw_gui();

		_current_command_buffer = util::nothing;

		// remove unused textures from cache
		for(auto& texture : _loaded_textures) {
			if(texture.use_count() == 1) {
				texture.reset();
			}
		}
		util::erase_if(_loaded_textures, [](auto& t) { return !t; });
		util::erase_if(_loaded_textures_by_aid, [](auto& t) { return t.second.expired(); });
		util::erase_if(_loaded_textures_by_handle, [](auto& t) { return t.second.expired(); });
	}

	Gui_pass::Loaded_texture::Loaded_texture(std::uintptr_t          handle,
	                                         graphic::Texture_ptr    texture,
	                                         vk::Sampler             sampler,
	                                         Deferred_renderer&      renderer,
	                                         vk::DescriptorSetLayout desc_layout)
	  : handle{reinterpret_cast<void*>(handle)}
	  , descriptor_set(renderer.create_descriptor_set(desc_layout, 1))
	  , texture(std::move(texture))
	  , sampler(sampler)
	  , renderer(&renderer)
	{
	}
	Gui_pass::Loaded_texture::~Loaded_texture()
	{
		renderer->device().destroy_after_frame(std::move(descriptor_set));
		renderer->device().destroy_after_frame(std::move(texture));
	}
	auto Gui_pass::Loaded_texture::get_if_ready() -> util::maybe<const graphic::DescriptorSet&>
	{
		if(!texture.ready())
			return util::nothing;

		if(!initialized) {
			auto desc_image = vk::DescriptorImageInfo{
			        sampler, this->texture->view(), vk::ImageLayout::eShaderReadOnlyOptimal};

			auto desc_write = vk::WriteDescriptorSet{
			        *descriptor_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &desc_image, nullptr};

			renderer->device().vk_device()->updateDescriptorSets(1, &desc_write, 0, nullptr);
			initialized = true;
		}

		return descriptor_set;
	}

	auto Gui_pass::load_texture(int width, int height, int channels, const std::uint8_t* data)
	        -> std::shared_ptr<void>
	{
		auto handle = _next_texture_handle++;

		auto dimensions = graphic::Image_dimensions_t<graphic::Image_type::single_2d>{
		        gsl::narrow<std::int32_t>(width), gsl::narrow<std::int32_t>(height)};

		auto texture = asset::make_ready_asset(
		        "tex:$in_memory_gui_texture$"_aid,
		        graphic::Texture_2D(_renderer.device(),
		                            dimensions,
		                            false,
		                            gsl::narrow<std::int32_t>(channels),
		                            true,
		                            gsl::span<const std::uint8_t>{data, width * height * channels},
		                            _renderer.queue_family()));

		auto entry = std::make_shared<Loaded_texture>(
		        handle, std::move(texture), *_sampler, _renderer, *_descriptor_set_layout);

		_loaded_textures_by_handle.emplace(handle, entry);

		auto return_value = std::shared_ptr<void>(entry, entry->handle);

		_loaded_textures.emplace_back(entry);

		return return_value;
	}

	auto Gui_pass::load_texture(const asset::AID& aid) -> std::shared_ptr<void>
	{
		auto cache_entry = _loaded_textures_by_aid[aid];
		if(auto sp = cache_entry.lock()) {
			return sp;
		}

		auto handle = _next_texture_handle++;

		auto entry =
		        std::make_shared<Loaded_texture>(handle,
		                                         _renderer.asset_manager().load<graphic::Texture_2D>(aid),
		                                         *_sampler,
		                                         _renderer,
		                                         *_descriptor_set_layout);

		_loaded_textures_by_handle.emplace(handle, entry);

		auto return_value = std::shared_ptr<void>(entry, entry->handle);

		_loaded_textures.emplace_back(entry);

		cache_entry = return_value;
		return return_value;
	}


	void Gui_pass::prepare_draw(std::size_t      index_count,
	                            std::size_t      vertex_count,
	                            glm::mat4        view_proj,
	                            Prepare_data_src write_data)
	{

		auto& cb = _current_command_buffer.get_or_throw(
		        "Gui_pass::prepare_draw ha to be "
		        "called inside a draw call!");
		auto& fb = _current_framebuffer.get_or_throw(
		        "Gui_pass::prepare_draw ha to be "
		        "called inside a draw call!");

		auto index_offset = vertex_count * sizeof(gui::Gui_vertex);

		_mesh_buffer.resize(gsl::narrow<std::int32_t>(index_offset + sizeof(std::int16_t) * index_count));

		_mesh_buffer.update_objects<char>(0, [&](gsl::span<char> data) {
			write_data(reinterpret_cast<std::uint16_t*>(data.data() + index_offset),
			           reinterpret_cast<gui::Gui_vertex*>(data.data()));
		});

		_mesh_buffer.flush(cb,
		                   vk::PipelineStageFlagBits::eVertexInput,
		                   vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eIndexRead);

		_render_pass.unsafe_begin_renderpass(cb, fb); // ended by finalize_draw()

		_render_pass.push_constant("camera"_strid, view_proj);

		cb.bindVertexBuffers(0, {_mesh_buffer.read_buffer()}, {0});
		cb.bindIndexBuffer(
		        _mesh_buffer.read_buffer(), gsl::narrow<std::uint32_t>(index_offset), vk::IndexType::eUint16);
	}

	void Gui_pass::draw_elements(void*         texture_handle,
	                             glm::vec4     clip_rect,
	                             std::uint32_t offset,
	                             std::uint32_t count,
	                             std::uint32_t vertex_offset)
	{
		auto int_tex_handle = reinterpret_cast<std::uintptr_t>(texture_handle);

		auto& cb = _current_command_buffer.get_or_throw(
		        "Gui_pass::prepare_draw has to be called inside a draw call!");

		if(_bound_texture_handle.is_nothing() || int_tex_handle != _bound_texture_handle.get_or_throw()) {
			auto texture = _loaded_textures_by_handle[int_tex_handle].lock();
			MIRRAGE_INVARIANT(texture,
			                  "The requested texture (" << int_tex_handle
			                                            << ") has not been loaded or already been freed!");

			auto descset = texture->get_if_ready();
			if(descset.is_nothing())
				return;

			_render_pass.bind_descriptor_sets(0, {descset.get_or_throw().get_ptr(), 1});
			_bound_texture_handle = int_tex_handle;
		}

		cb.setScissor(0,
		              {vk::Rect2D{vk::Offset2D(static_cast<std::int32_t>(clip_rect.x),
		                                       static_cast<std::int32_t>(clip_rect.y)),
		                          vk::Extent2D(static_cast<std::uint32_t>(clip_rect.z - clip_rect.x),
		                                       static_cast<std::uint32_t>(clip_rect.w - clip_rect.y))}});

		cb.drawIndexed(count, 1, offset, std::int32_t(vertex_offset), 0);
	}

	void Gui_pass::finalize_draw() { _render_pass.unsafe_end_renderpass(); }


	auto Gui_pass_factory::create_pass(Deferred_renderer& renderer,
	                                   util::maybe<ecs::Entity_manager&>,
	                                   Engine& engine,
	                                   bool&) -> std::unique_ptr<Render_pass>
	{
		return std::make_unique<gui::Gui_renderer_instance<Gui_pass>>(engine.gui(), renderer, engine);
	}

	auto Gui_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Gui_pass_factory::configure_device(vk::PhysicalDevice,
	                                        util::maybe<std::uint32_t>,
	                                        graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
