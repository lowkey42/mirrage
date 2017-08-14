#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/ecs/ecs.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/window.hpp>

#include <gsl/gsl>
#include <glm/glm.hpp>

using namespace mirrage::graphic;

namespace mirrage {
namespace renderer {

	Deferred_renderer::Deferred_renderer(Deferred_renderer_factory& factory,
	                                     std::vector<std::unique_ptr<Pass_factory>>& passes,
	                                     ecs::Entity_manager& ecs,
	                                     util::maybe<Meta_system&> userdata)
	    : _factory(factory)
	    , _descriptor_set_pool(device().create_descriptor_pool(256, {
	          {vk::DescriptorType::eUniformBuffer, 8},
	          {vk::DescriptorType::eCombinedImageSampler, 256},
	          {vk::DescriptorType::eInputAttachment, 64},
	          {vk::DescriptorType::eSampledImage, 256},
	          {vk::DescriptorType::eSampler, 64}
	      }))
	    , _gbuffer(device(), factory._window.width(), factory._window.height())
	    , _profiler(device(), 64)

	    , _texture_cache(device(), device().get_queue_family("draw"_strid))
	    , _model_loader(device(), device().get_queue_family("draw"_strid), _texture_cache, 64)

	    , _global_uniform_descriptor_set_layout(device().create_descriptor_set_layout(
	          vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1,
	                                         vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment}
	      ))
	    , _global_uniform_descriptor_set(_descriptor_set_pool.create_descriptor(
	                                         *_global_uniform_descriptor_set_layout))
	    , _global_uniform_buffer(device().transfer().create_dynamic_buffer(
	          sizeof(Global_uniforms), vk::BufferUsageFlagBits::eUniformBuffer,
	          vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eUniformRead,
	          vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eUniformRead
	      ))
	    , _passes(util::map(passes, [&, write_first_pp_buffer=true](auto& factory)mutable {
	          return factory->create_pass(*this, ecs, userdata, write_first_pp_buffer);
	      }))
	    , _cameras(ecs.list<Camera_comp>()) {

		_write_global_uniform_descriptor_set();
	}
	Deferred_renderer::~Deferred_renderer() {
		device().print_memory_usage(std::cout);
		device().wait_idle();
		_passes.clear();
	}

	void Deferred_renderer::_write_global_uniform_descriptor_set() {
		auto buffer_info = vk::DescriptorBufferInfo(_global_uniform_buffer.buffer(),
		                                             0, sizeof(Global_uniforms));

		auto desc_writes = std::array<vk::WriteDescriptorSet,1>();
		desc_writes[0] = vk::WriteDescriptorSet{*_global_uniform_descriptor_set, 0, 0,
		                                        1, vk::DescriptorType::eUniformBuffer,
		                                        nullptr, &buffer_info};

		device().vk_device()->updateDescriptorSets(desc_writes.size(), desc_writes.data(), 0, nullptr);
	}

	void Deferred_renderer::update(util::Time dt) {
		_time_acc += dt.value();
		_delta_time = dt.value();

		for(auto& pass : _passes) {
			pass->update(dt);
		}
	}
	void Deferred_renderer::draw() {
		if(active_camera().is_nothing())
			return;

		auto main_command_buffer = _factory.queue_temporary_command_buffer();
		main_command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
		_profiler.start(main_command_buffer);
		ON_EXIT {
			_profiler.end();
			main_command_buffer.end();
		};

		_update_global_uniforms(main_command_buffer, active_camera().get_or_throw());

		auto command_buffer_src = std::function<vk::CommandBuffer()>([&]() -> vk::CommandBuffer {
			return _factory.queue_temporary_command_buffer();
		});

		auto swapchain_image_idx = _factory._aquire_next_image();

		// draw subpasses
		for(auto& pass : _passes) {
			auto _ = _profiler.push(pass->name());

			pass->draw(main_command_buffer, command_buffer_src,
			           *_global_uniform_descriptor_set, swapchain_image_idx);
		}

		// reset cached camera state
		_active_camera = util::nothing;
	}

	void Deferred_renderer::shrink_to_fit() {
		for(auto& pass : _passes) {
			pass->shrink_to_fit();
		}

		_model_loader.shrink_to_fit();
		_texture_cache.shrink_to_fit();
		device().shrink_to_fit();
		device().print_memory_usage(std::cout);
	}

	void Deferred_renderer::_update_global_uniforms(vk::CommandBuffer cb, const Camera_state& camera) {
		_global_uniforms.eye_pos = glm::vec4(camera.eye_position, 1.f);
		_global_uniforms.view_proj_mat = camera.view_projection;
		_global_uniforms.view_mat = camera.view;
		_global_uniforms.proj_mat = camera.projection;
		_global_uniforms.inv_view_mat = camera.inv_view;
		_global_uniforms.inv_proj_mat = glm::inverse(camera.projection);
		_global_uniforms.proj_planes.x = camera.near_plane;
		_global_uniforms.proj_planes.y = camera.far_plane;
		_global_uniforms.proj_planes.z = camera.fov_horizontal;
		_global_uniforms.proj_planes.w = camera.fov_vertical;
		_global_uniforms.time = glm::vec4(_time_acc, glm::sin(_time_acc), _delta_time, 0);
		_global_uniforms.proj_info = glm::vec4(
				-2.f / camera.projection[0][0],
				-2.f / camera.projection[1][1],
				(1.f - camera.projection[0][2]) / camera.projection[0][0],
				(1.f + camera.projection[1][2]) / camera.projection[1][1]
		);
		_global_uniform_buffer.update_obj(cb, _global_uniforms);
	}

	auto Deferred_renderer::active_camera()noexcept -> util::maybe<Camera_state&> {
		if(_active_camera.is_some())
			return _active_camera.get_or_throw();

		auto max_prio = std::numeric_limits<float>::lowest();
		auto active   = (Camera_comp*) nullptr;

		for(auto& camera : _cameras) {
			if(camera.priority() > max_prio) {
				max_prio = camera.priority();
				active = &camera;
			}
		}

		if(active) {
			const auto& viewport = _factory._window.viewport();	
			_active_camera = Camera_state(*active, viewport);
			
			for(auto& p : _passes) {
				p->process_camera(_active_camera.get_or_throw());
			}
			
			return _active_camera.get_or_throw();

		} else {
			_active_camera = util::nothing;
			return util::nothing;
		}
	}

	auto Deferred_renderer::create_descriptor_set(vk::DescriptorSetLayout layout
	                                              ) -> vk::UniqueDescriptorSet {
		return _descriptor_set_pool.create_descriptor(layout);
	}


	Deferred_renderer_factory::Deferred_renderer_factory(graphic::Context& context,
	                                                     graphic::Window& window,
	                                                     std::vector<std::unique_ptr<Pass_factory>> passes)
	    : _pass_factories(std::move(passes))
	    , _window(window)
	    , _device(context.instantiate_device(FOE_SELF(_rank_device), FOE_SELF(_init_device),
	                                         {&_window}, true))
	    , _swapchain(_device->get_single_swapchain())
	    , _queue_family(_device->get_queue_family("draw"_strid))
	    , _queue(_device->get_queue("draw"_strid))
	    , _image_acquired(_device->create_semaphore())
	    , _image_presented(_device->create_semaphore())
	    , _command_buffer_pool(_device->create_command_buffer_pool("draw"_strid, true, true)) {
		
		auto maybe_settings = context.asset_manager().load_maybe<Renderer_settings>("cfg:renderer"_aid);
		if(maybe_settings.is_nothing()) {
			settings({});
		} else {
			_settings = maybe_settings.get_or_throw();
		}
	}
	
	void Deferred_renderer_factory::settings(const Renderer_settings& s) {
		auto& assets = _device->context().asset_manager();
		assets.save<Renderer_settings>("cfg:renderer"_aid, s);
		_settings = assets.load<Renderer_settings>("cfg:renderer"_aid);
	}

	auto Deferred_renderer_factory::create_renderer(ecs::Entity_manager& ecs,
	                                                util::maybe<Meta_system&> userdata
	                                                ) -> std::unique_ptr<Deferred_renderer> {
		return std::make_unique<Deferred_renderer>(*this, _pass_factories, ecs, userdata);
	}

	void Deferred_renderer_factory::finish_frame() {
		if(_aquired_swapchain_image.is_nothing())
			return; // nothing drawn, nothing to do


		auto transfer_barriers = *_device->destroy_after_frame(std::move(_command_buffer_pool.create_primary()[0]));
		auto ff = _device->finish_frame(transfer_barriers);
		auto& frame_fence = std::get<0>(ff);
		auto& transfer_semaphore = std::get<1>(ff);

		// submit queue
		auto wait_semaphores = std::array<vk::Semaphore, 2>{};
		auto wait_stages     = std::array<vk::PipelineStageFlags, 2>{};
		auto wait_count      = std::uint32_t(1);
		wait_semaphores[0] = *_image_acquired;
		wait_stages[0]     = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		if(transfer_semaphore.is_some()) {
			wait_count++;
			wait_semaphores[1] = transfer_semaphore.get_or_throw();
			wait_stages[1] = vk::PipelineStageFlagBits::eAllCommands;
			_queued_commands.insert(_queued_commands.begin(), transfer_barriers);
		}

		auto submit_info = vk::SubmitInfo {
			wait_count, wait_semaphores.data(), wait_stages.data(),
			gsl::narrow<std::uint32_t>(_queued_commands.size()), _queued_commands.data(),
			1, &*_image_presented
		};

		_queue.submit({submit_info}, frame_fence);
		_queued_commands.clear();

		// present
		_swapchain.present(_queue, _aquired_swapchain_image.get_or_throw(), *_image_presented);
		_aquired_swapchain_image = util::nothing;
		_window.on_present();
	}

	auto Deferred_renderer_factory::_rank_device(vk::PhysicalDevice gpu,
	                                             util::maybe<std::uint32_t> gqueue) -> int {
		auto properties = gpu.getProperties();
		auto features = gpu.getFeatures();

		auto score = 0;

		if(properties.deviceType==vk::PhysicalDeviceType::eDiscreteGpu) {
			score += 200;
		} else if(properties.deviceType==vk::PhysicalDeviceType::eIntegratedGpu) {
			score += 100;
		}

		if(!features.geometryShader) {
			return std::numeric_limits<int>::min();
		}

		if(gqueue.is_nothing()) {
			return std::numeric_limits<int>::min();
		}

		for(auto& pass : _pass_factories) {
			score = pass->rank_device(gpu, gqueue, score);
		}

		return score;
	}

	auto Deferred_renderer_factory::_init_device(vk::PhysicalDevice gpu,
	                                             util::maybe<std::uint32_t> gqueue
	                                             ) -> graphic::Device_create_info {
		auto ret_val = Device_create_info{};

		INVARIANT(gqueue.is_some(), "No useable queue family");
		ret_val.queue_families.emplace("draw"_strid, Queue_create_info{gqueue.get_or_throw()});

		auto supported_features = gpu.getFeatures();
		INVARIANT(supported_features.samplerAnisotropy, "Anisotropic filtering is not supported by device!");
		ret_val.features.samplerAnisotropy = true;

		for(auto& pass : _pass_factories) {
			pass->configure_device(gpu, gqueue, ret_val);
		}

		return ret_val;
	}

	auto Deferred_renderer_factory::_aquire_next_image() -> std::size_t {
		if(_aquired_swapchain_image.is_some())
			return _aquired_swapchain_image.get_or_throw();

		_aquired_swapchain_image = _swapchain.acquireNextImage(*_image_acquired, {});

		return _aquired_swapchain_image.get_or_throw();
	}

	void Deferred_renderer_factory::queue_commands(vk::CommandBuffer cmd) {
		_queued_commands.emplace_back(cmd);
	}
	auto Deferred_renderer_factory::queue_temporary_command_buffer() -> vk::CommandBuffer {
		auto cb = *_device->destroy_after_frame(std::move(_command_buffer_pool.create_primary()[0]));
		queue_commands(cb);

		return cb;
	}

}
}
