#include <mirrage/renderer/deferred_renderer.hpp>

#include "debug_ui.hpp"

#include <mirrage/asset/embedded_asset.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/engine.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/window.hpp>
#include <mirrage/gui/debug_ui.hpp>
#include <mirrage/gui/gui.hpp>

#include <glm/glm.hpp>
#include <gsl/gsl>

using namespace mirrage::graphic;

extern void ref_embedded_assets_mirrage_renderer();

namespace mirrage::renderer {

	namespace {
		auto gbuffer_required(const std::vector<Render_pass_factory*>& passes)
		{
			return std::any_of(passes.begin(), passes.end(), [&](auto& p) { return p->requires_gbuffer(); });
		}

		auto create_billboard_model(Deferred_renderer& r)
		{
			const auto vertices =
			        std::array<Model_vertex, 4>{Model_vertex{glm::vec3(-0.5f, -0.5f, 0),
			                                                 glm::normalize(glm::vec3(-0.8f, -0.8f, 0.131f)),
			                                                 glm::vec2(0, 1)},
			                                    Model_vertex{glm::vec3(0.5f, -0.5f, 0),
			                                                 glm::normalize(glm::vec3(0.8f, -0.8f, 0.131f)),
			                                                 glm::vec2(1, 1)},
			                                    Model_vertex{glm::vec3(-0.5f, 0.5f, 0),
			                                                 glm::normalize(glm::vec3(-0.8f, 0.8f, 0.131f)),
			                                                 glm::vec2(0, 0)},
			                                    Model_vertex{glm::vec3(0.5f, 0.5f, 0),
			                                                 glm::normalize(glm::vec3(0.8f, 0.8f, 0.131f)),
			                                                 glm::vec2(1, 0)}};
			const auto indices = std::array<std::uint32_t, 6>{0, 1, 2, 2, 1, 3};

			return Model{graphic::Mesh{r.device(), r.queue_family(), vertices, indices},
			             {Sub_mesh{{}, 0u, 6u, glm::vec3(0, 0, 0), 1.f}},
			             1.f,
			             glm::vec3(0, 0, 0),
			             false,
			             0};
		}
	} // namespace


	Deferred_renderer::Deferred_renderer(Deferred_renderer_factory&        factory,
	                                     std::vector<Render_pass_factory*> passes,
	                                     util::maybe<ecs::Entity_manager&> ecs,
	                                     Engine&                           engine)
	  : _engine(&engine)
	  , _factory(&factory)
	  , _entity_manager(ecs)
	  , _descriptor_set_pool(*device().vk_device(),
	                         128,
	                         {vk::DescriptorType::eUniformBuffer,
	                          vk::DescriptorType::eUniformBufferDynamic,
	                          vk::DescriptorType::eCombinedImageSampler,
	                          vk::DescriptorType::eInputAttachment,
	                          vk::DescriptorType::eStorageBuffer,
	                          vk::DescriptorType::eStorageTexelBuffer,
	                          vk::DescriptorType::eStorageImage,
	                          vk::DescriptorType::eSampledImage,
	                          vk::DescriptorType::eSampler})
	  , _gbuffer(gbuffer_required(passes) ? std::make_unique<GBuffer>(device(),
	                                                                  _descriptor_set_pool,
	                                                                  factory._window.width(),
	                                                                  factory._window.height())
	                                      : std::unique_ptr<GBuffer>())
	  , _profiler(device(), 128)

	  , _global_uniform_descriptor_set(
	            _descriptor_set_pool.create_descriptor(*factory._global_uniform_descriptor_set_layout, 1))
	  , _global_uniform_buffer(
	            device().transfer().create_dynamic_buffer(sizeof(Global_uniforms),
	                                                      vk::BufferUsageFlagBits::eUniformBuffer,
	                                                      vk::PipelineStageFlagBits::eVertexShader,
	                                                      vk::AccessFlagBits::eUniformRead,
	                                                      vk::PipelineStageFlagBits::eFragmentShader,
	                                                      vk::AccessFlagBits::eUniformRead))
	  , _blue_noise(asset_manager().load<graphic::Texture_2D>("tex:blue_noise"_aid))
	  , _noise_sampler(device().create_sampler(1,
	                                           vk::SamplerAddressMode::eRepeat,
	                                           vk::BorderColor::eIntOpaqueBlack,
	                                           vk::Filter::eNearest,
	                                           vk::SamplerMipmapMode::eNearest))
	  , _noise_descriptor_set_layout(device(), *_noise_sampler, 1, vk::ShaderStageFlagBits::eFragment)
	  , _billboard_model(create_billboard_model(*this))
	  , _pass_factories(std::move(passes))
	  , _passes(util::map(_pass_factories,
	                      [&, write_first_pp_buffer = true](auto& factory) mutable {
		                      return factory->create_pass(*this, ecs, engine, write_first_pp_buffer);
	                      }))
	  , _cameras(ecs.is_some() ? util::justPtr(&ecs.get_or_throw().list<Camera_comp>()) : util::nothing)
	{
		if(ecs.is_some())
			ecs.get_or_throw().register_component_type<Material_property_comp>();

		ref_embedded_assets_mirrage_renderer();

		_write_global_uniform_descriptor_set();

		factory._renderer_instances.emplace_back(this);
	}
	Deferred_renderer::~Deferred_renderer()
	{
		util::erase_fast(_factory->_renderer_instances, this);

		device().print_memory_usage(std::cout);
		device().wait_idle();
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		_passes.clear();
		device().wait_idle(true);
	}

	void Deferred_renderer::recreate()
	{
		LOG(plog::warning) << "--recreate";
		device().wait_idle();

		for(auto& pass : _passes) {
			if(pass)
				pass.reset();
		}

		if(gbuffer_required(_pass_factories)) {
			_gbuffer.reset();

			// recreate gbuffer and renderpasses
			_gbuffer = std::make_unique<GBuffer>(
			        device(), _descriptor_set_pool, _factory->_window.width(), _factory->_window.height());
		}

		auto write_first_pp_buffer = true;
		for(auto i = std::size_t(0); i < _passes.size(); i++) {
			_passes[i] = _pass_factories.at(i)->create_pass(
			        *this, _entity_manager, *_engine, write_first_pp_buffer);
		}

		_profiler = graphic::Profiler(device(), 64);

		device().wait_idle();
	}

	void Deferred_renderer::_write_global_uniform_descriptor_set()
	{
		auto buffer_info =
		        vk::DescriptorBufferInfo(_global_uniform_buffer.buffer(), 0, sizeof(Global_uniforms));

		auto desc_writes = std::array<vk::WriteDescriptorSet, 1>();
		desc_writes[0]   = vk::WriteDescriptorSet{*_global_uniform_descriptor_set,
                                                0,
                                                0,
                                                1,
                                                vk::DescriptorType::eUniformBuffer,
                                                nullptr,
                                                &buffer_info};

		device().vk_device()->updateDescriptorSets(
		        gsl::narrow<uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);
	}

	void Deferred_renderer::update(util::Time dt)
	{
		_time_acc += dt.value();
		_delta_time    = dt.value();
		_frame_counter = (_frame_counter + 1) % 10;

		for(auto& pass : _passes) {
			if(pass)
				pass->update(dt);
		}
	}
	void Deferred_renderer::draw()
	{
		if(!_noise_descriptor_set) {
			if(_blue_noise.ready()) {
				LOG(plog::debug) << "Noise texture loaded";
				_noise_descriptor_set =
				        _noise_descriptor_set_layout.create_set(descriptor_pool(), {_blue_noise->view()});
			} else {
				// texture not loaded, skip frame
				return;
			}
		}

		if(active_camera().is_nothing())
			return;

		auto main_command_buffer = _factory->queue_temporary_command_buffer();
		main_command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
		_profiler.start(main_command_buffer);
		ON_EXIT
		{
			_profiler.end();
			main_command_buffer.end();
		};

		_update_global_uniforms(main_command_buffer, active_camera().get_or_throw());

		_frame_data.main_command_buffer = main_command_buffer;
		_frame_data.global_uniform_set  = *_global_uniform_descriptor_set;
		_frame_data.swapchain_image     = _factory->_aquire_next_image();

		// draw subpasses
		for(auto& pass : _passes) {
			if(pass) {
				auto q = graphic::Queue_debug_label(device().context(), main_command_buffer, pass->name());
				auto _ = _profiler.push(pass->name());
				pass->draw(_frame_data);
			}
		}

		_frame_data.clear_queues();

		// reset cached camera state
		_active_camera = util::nothing;
	}

	void Deferred_renderer::shrink_to_fit()
	{
		device().shrink_to_fit();
		device().print_memory_usage(std::cout);
	}

	void Deferred_renderer::_update_global_uniforms(vk::CommandBuffer cb, const Camera_state& camera)
	{
		_global_uniforms.eye_pos       = glm::vec4(camera.eye_position, 1.f);
		_global_uniforms.view_proj_mat = camera.view_projection;
		_global_uniforms.view_mat      = camera.view;
		_global_uniforms.proj_mat      = camera.projection;
		_global_uniforms.inv_view_mat  = camera.inv_view;
		_global_uniforms.inv_proj_mat  = glm::inverse(camera.projection);
		_global_uniforms.proj_planes.x = camera.near_plane;
		_global_uniforms.proj_planes.y = camera.far_plane;
		_global_uniforms.proj_planes.z = camera.fov_horizontal;
		_global_uniforms.proj_planes.w = camera.fov_vertical;
		_global_uniforms.time      = glm::vec4(_time_acc, glm::sin(_time_acc), _delta_time, _frame_counter);
		_global_uniforms.proj_info = glm::vec4(-2.f / camera.projection[0][0],
		                                       -2.f / camera.projection[1][1],
		                                       (1.f - camera.projection[0][2]) / camera.projection[0][0],
		                                       (1.f + camera.projection[1][2]) / camera.projection[1][1]);
		_global_uniform_buffer.update_obj(cb, _global_uniforms);
	}

	auto Deferred_renderer::active_camera() noexcept -> util::maybe<Camera_state&>
	{
		if(_active_camera.is_some())
			return _active_camera.get_or_throw();

		if(_cameras.is_some()) {
			auto max_prio = std::numeric_limits<float>::lowest();
			auto active   = static_cast<Camera_comp*>(nullptr);

			for(auto& camera : _cameras.get_or_throw()) {
				if(camera.priority() > max_prio && camera.owner(_entity_manager.get_or_throw()).is_some()) {
					max_prio = camera.priority();
					active   = &camera;
				}
			}

			if(active) {
				const auto& viewport  = _factory->_window.viewport();
				auto&       transform = active->owner(_entity_manager.get_or_throw())
				                          .get_or_throw()
				                          .get<ecs::components::Transform_comp>()
				                          .get_or_throw("Camera without transform component!");
				_active_camera = Camera_state(*active, transform, viewport);

				for(auto& p : _passes) {
					if(p)
						p->process_camera(_active_camera.get_or_throw());
				}

				return util::justPtr(&_active_camera.get_or_throw());
			} else {
				_active_camera = util::nothing;
				return util::maybe<Camera_state&>();
			}

		} else {
			_active_camera.emplace(_factory->_window.viewport());
			return util::justPtr(&_active_camera.get_or_throw());
		}
	}

	auto Deferred_renderer::create_descriptor_set(vk::DescriptorSetLayout layout, std::int32_t bindings)
	        -> graphic::DescriptorSet
	{
		return _descriptor_set_pool.create_descriptor(layout, bindings);
	}

	void Deferred_renderer::debug_draw_sphere(const glm::vec3& center, float radius, const util::Rgb& color)
	{
		constexpr auto vertices = std::array<glm::vec3, 42>{
		        glm::vec3(0.000000, -1.000000, 0.000000),   glm::vec3(0.723607, -0.447220, 0.525725),
		        glm::vec3(-0.276388, -0.447220, 0.850649),  glm::vec3(-0.894426, -0.447216, 0.000000),
		        glm::vec3(-0.276388, -0.447220, -0.850649), glm::vec3(0.723607, -0.447220, -0.525725),
		        glm::vec3(0.276388, 0.447220, 0.850649),    glm::vec3(-0.723607, 0.447220, 0.525725),
		        glm::vec3(-0.723607, 0.447220, -0.525725),  glm::vec3(0.276388, 0.447220, -0.850649),
		        glm::vec3(0.894426, 0.447216, 0.000000),    glm::vec3(0.000000, 1.000000, 0.000000),
		        glm::vec3(-0.162456, -0.850654, 0.499995),  glm::vec3(0.425323, -0.850654, 0.309011),
		        glm::vec3(0.262869, -0.525738, 0.809012),   glm::vec3(0.850648, -0.525736, 0.000000),
		        glm::vec3(0.425323, -0.850654, -0.309011),  glm::vec3(-0.525730, -0.850652, 0.000000),
		        glm::vec3(-0.688189, -0.525736, 0.499997),  glm::vec3(-0.162456, -0.850654, -0.499995),
		        glm::vec3(-0.688189, -0.525736, -0.499997), glm::vec3(0.262869, -0.525738, -0.809012),
		        glm::vec3(0.951058, 0.000000, 0.309013),    glm::vec3(0.951058, 0.000000, -0.309013),
		        glm::vec3(0.000000, 0.000000, 1.000000),    glm::vec3(0.587786, 0.000000, 0.809017),
		        glm::vec3(-0.951058, 0.000000, 0.309013),   glm::vec3(-0.587786, 0.000000, 0.809017),
		        glm::vec3(-0.587786, 0.000000, -0.809017),  glm::vec3(-0.951058, 0.000000, -0.309013),
		        glm::vec3(0.587786, 0.000000, -0.809017),   glm::vec3(0.000000, 0.000000, -1.000000),
		        glm::vec3(0.688189, 0.525736, 0.499997),    glm::vec3(-0.262869, 0.525738, 0.809012),
		        glm::vec3(-0.850648, 0.525736, 0.000000),   glm::vec3(-0.262869, 0.525738, -0.809012),
		        glm::vec3(0.688189, 0.525736, -0.499997),   glm::vec3(0.162456, 0.850654, 0.499995),
		        glm::vec3(0.525730, 0.850652, 0.000000),    glm::vec3(-0.425323, 0.850654, 0.309011),
		        glm::vec3(-0.425323, 0.850654, -0.309011),  glm::vec3(0.162456, 0.850654, -0.499995)};

		if(_factory->settings().debug_geometry) {
			const auto line = [&](auto b, auto e) {
				_frame_data.debug_geometry_queue.emplace_back(vertices[std::size_t(b - 1)] * radius + center,
				                                              vertices[std::size_t(e - 1)] * radius + center,
				                                              color);
			};

			line(1, 13);
			line(1, 14);
			line(1, 17);
			line(1, 18);
			line(1, 20);
			line(2, 14);
			line(2, 15);
			line(2, 16);
			line(2, 23);
			line(2, 26);
			line(3, 13);
			line(3, 15);
			line(3, 19);
			line(3, 25);
			line(3, 28);
			line(4, 18);
			line(4, 19);
			line(4, 21);
			line(4, 27);
			line(4, 30);
			line(5, 20);
			line(5, 21);
			line(5, 22);
			line(5, 29);
			line(5, 32);
			line(6, 16);
			line(6, 17);
			line(6, 22);
			line(6, 24);
			line(6, 31);
			line(7, 25);
			line(7, 26);
			line(7, 33);
			line(7, 34);
			line(7, 38);
			line(8, 27);
			line(8, 28);
			line(8, 34);
			line(8, 35);
			line(8, 40);
			line(9, 29);
			line(9, 30);
			line(9, 35);
			line(9, 36);
			line(9, 41);
			line(10, 31);
			line(10, 32);
			line(10, 36);
			line(10, 37);
			line(10, 42);
			line(11, 23);
			line(11, 24);
			line(11, 33);
			line(11, 37);
			line(11, 39);
			line(12, 38);
			line(12, 39);
			line(12, 40);
			line(12, 41);
			line(12, 42);
			line(13, 14);
			line(13, 15);
			line(13, 18);
			line(13, 19);
			line(14, 15);
			line(14, 16);
			line(14, 17);
			line(15, 25);
			line(15, 26);
			line(16, 17);
			line(16, 23);
			line(16, 24);
			line(17, 20);
			line(17, 22);
			line(18, 19);
			line(18, 20);
			line(18, 21);
			line(19, 27);
			line(19, 28);
			line(20, 21);
			line(20, 22);
			line(21, 29);
			line(21, 30);
			line(22, 31);
			line(22, 32);
			line(23, 24);
			line(23, 26);
			line(23, 33);
			line(24, 31);
			line(24, 37);
			line(25, 26);
			line(25, 28);
			line(25, 34);
			line(27, 28);
			line(27, 30);
			line(27, 35);
			line(28, 34);
			line(29, 30);
			line(29, 32);
			line(29, 36);
			line(30, 35);
			line(31, 32);
			line(31, 37);
			line(32, 36);
			line(33, 38);
			line(33, 39);
			line(34, 38);
			line(34, 40);
			line(35, 40);
			line(35, 41);
			line(36, 41);
			line(36, 42);
			line(37, 39);
			line(37, 42);
			line(38, 39);
			line(38, 40);
			line(39, 42);
			line(40, 41);
			line(41, 42);
		}
	}


	struct Deferred_renderer_factory::Asset_loaders {
		asset::Asset_manager& assets;

		Asset_loaders(asset::Asset_manager&   assets,
		              graphic::Device&        device,
		              vk::Sampler             material_sampler,
		              vk::DescriptorSetLayout material_layout,
		              std::uint32_t           draw_queue,
		              vk::DescriptorSetLayout storage_buffer,
		              vk::DescriptorSetLayout uniform_buffer)
		  : assets(assets)
		{
			assets.create_stateful_loader<Material>(device, assets, material_sampler, material_layout);
			assets.create_stateful_loader<Model>(device, assets, draw_queue);
			assets.create_stateful_loader<Particle_script>(device, storage_buffer, uniform_buffer);
			assets.create_stateful_loader<Particle_system_config>();
			assets.create_stateful_loader<Particle_type_config>();
		}
		~Asset_loaders()
		{
			assets.remove_stateful_loader<Particle_type_config>();
			assets.remove_stateful_loader<Particle_system_config>();
			assets.remove_stateful_loader<Particle_script>();
			assets.remove_stateful_loader<Model>();
			assets.remove_stateful_loader<Material>();
		}
	};

	Deferred_renderer_factory::Deferred_renderer_factory(
	        Engine& engine, graphic::Window& window, std::vector<std::unique_ptr<Render_pass_factory>> passes)
	  : _engine(engine)
	  , _assets(engine.assets())
	  , _pass_factories(std::move(passes))
	  , _window(window)
	  , _device(engine.graphics_context().instantiate_device(
	            FOE_SELF(_rank_device), FOE_SELF(_init_device), {&_window}, true))
	  , _swapchain(_device->get_single_swapchain())
	  , _draw_queue_family(_device->get_queue_family("draw"_strid))
	  , _compute_queue_family(_device->get_queue_family("compute"_strid))
	  , _draw_queue(_device->get_queue("draw"_strid))
	  , _compute_queue(_device->get_queue("compute"_strid))
	  , _image_acquired(_device->create_semaphore())
	  , _image_presented(_device->create_semaphore())
	  , _command_buffer_pool(_device->create_command_buffer_pool("draw"_strid, true, true))
	  , _compute_command_buffer_pool(_device->create_command_buffer_pool("compute"_strid, true, true))
	  , _model_material_sampler(_device->create_sampler(12))
	  , _model_desc_set_layout(create_material_descriptor_set_layout(*_device, *_model_material_sampler))
	  , _global_uniform_descriptor_set_layout(
	            _device->create_descriptor_set_layout(vk::DescriptorSetLayoutBinding{
	                    0,
	                    vk::DescriptorType::eUniformBuffer,
	                    1,
	                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
	                            | vk::ShaderStageFlagBits::eCompute}))

	  , _compute_storage_buffer_layout(_device->create_descriptor_set_layout(vk::DescriptorSetLayoutBinding{
	            0,
	            vk::DescriptorType::eStorageBuffer,
	            1,
	            vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex
	                    | vk::ShaderStageFlagBits::eFragment}))
	  , _compute_uniform_buffer_layout(_device->create_descriptor_set_layout(vk::DescriptorSetLayoutBinding{
	            0,
	            vk::DescriptorType::eUniformBuffer,
	            1,
	            vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex
	                    | vk::ShaderStageFlagBits::eFragment}))
	  , _asset_loaders(std::make_unique<Asset_loaders>(_assets,
	                                                   *_device,
	                                                   *_model_material_sampler,
	                                                   *_model_desc_set_layout,
	                                                   _draw_queue_family,
	                                                   compute_storage_buffer_layout(),
	                                                   compute_uniform_buffer_layout()))
	  , _all_passes_mask(util::map(_pass_factories, [&](auto& f) { return f->id(); }))
	  , _profiler_menu(std::make_unique<Profiler_menu>(_renderer_instances))
	  , _settings_menu(std::make_unique<Settings_menu>(*this, _window))
	{
		auto maybe_settings = _assets.load_maybe<Renderer_settings>("cfg:renderer"_aid);
		if(maybe_settings.is_nothing()) {
			_settings = asset::make_ready_asset("cfg:renderer"_aid, Renderer_settings{});
			save_settings();
		} else {
			_settings = maybe_settings.get_or_throw();
		}
	}
	Deferred_renderer_factory::~Deferred_renderer_factory() = default;

	void Deferred_renderer_factory::settings(const Renderer_settings& s, bool apply)
	{
		_settings = asset::make_ready_asset("cfg:renderer"_aid, s);

		_recreation_pending |= apply;
	}
	void Deferred_renderer_factory::save_settings()
	{
		_assets.save<Renderer_settings>("cfg:renderer"_aid, *_settings);
		_settings = _assets.load<Renderer_settings>("cfg:renderer"_aid);
	}

	auto Deferred_renderer_factory::create_renderer(util::maybe<ecs::Entity_manager&> ecs,
	                                                Render_pass_mask                  passes)
	        -> std::unique_ptr<Deferred_renderer>
	{
		auto pass_factories = std::vector<Render_pass_factory*>();
		pass_factories.reserve(_pass_factories.size());
		for(auto& p : _pass_factories) {
			if(passes.empty() || std::find(passes.begin(), passes.end(), p->id()) != passes.end()) {
				pass_factories.emplace_back(p.get());
			}
		}

		return std::make_unique<Deferred_renderer>(*this, pass_factories, ecs, _engine);
	}

	void Deferred_renderer_factory::finish_frame()
	{
		_present();

		if(_recreation_pending) {
			_recreation_pending = false;

			for(auto& inst : _renderer_instances) {
				inst->recreate();
			}
		}
	}
	void Deferred_renderer_factory::_present()
	{
		if(_aquired_swapchain_image.is_nothing()) {
			auto& image = _swapchain.get_images().at(_aquire_next_image());
			auto  cb    = queue_temporary_command_buffer();
			cb.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
			ON_EXIT { cb.end(); };

			graphic::clear_texture(cb,
			                       image,
			                       _swapchain.image_width(),
			                       _swapchain.image_height(),
			                       util::Rgba{0, 0, 0, 0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::ePresentSrcKHR,
			                       0,
			                       1);
			//return; // nothing drawn, nothing to do
		}


		auto transfer_barriers =
		        *_device->destroy_after_frame(std::move(_command_buffer_pool.create_primary()[0]));
		auto  ff                 = _device->finish_frame(transfer_barriers);
		auto& frame_fence        = std::get<0>(ff);
		auto& transfer_semaphore = std::get<1>(ff);

		// submit queue
		auto wait_semaphores = std::array<vk::Semaphore, 2>{};
		auto wait_stages     = std::array<vk::PipelineStageFlags, 2>{};
		auto wait_count      = std::uint32_t(1);
		wait_semaphores[0]   = *_image_acquired;
		wait_stages[0]       = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		if(transfer_semaphore.is_some()) {
			wait_count++;
			wait_semaphores[1] = transfer_semaphore.get_or_throw();
			wait_stages[1]     = vk::PipelineStageFlagBits::eAllCommands;
			_queued_commands.insert(_queued_commands.begin(), transfer_barriers);
		}

		auto submit_info = vk::SubmitInfo{wait_count,
		                                  wait_semaphores.data(),
		                                  wait_stages.data(),
		                                  gsl::narrow<std::uint32_t>(_queued_commands.size()),
		                                  _queued_commands.data(),
		                                  1,
		                                  &*_image_presented};

		_draw_queue.submit({submit_info}, frame_fence);
		_queued_commands.clear();

		// present
		if(_swapchain.present(_draw_queue, _aquired_swapchain_image.get_or_throw(), *_image_presented)) {
			_recreation_pending = true;
		}

		_aquired_swapchain_image = util::nothing;
	}

	namespace {
		auto find_compute_queue(vk::PhysicalDevice& gpu) -> util::maybe<std::uint32_t>
		{
			// try to find async compute queue first
			auto queue = find_queue_family(gpu, [](auto&& q) {
				return q.timestampValidBits >= 32 && q.queueFlags == vk::QueueFlagBits::eCompute;
			});

			// ... if their is none, fallback to any compute queue
			if(queue.is_nothing()) {
				queue = find_queue_family(gpu, [](auto&& q) {
					return q.timestampValidBits >= 32
					       && (q.queueFlags & vk::QueueFlagBits::eCompute) == vk::QueueFlagBits::eCompute;
				});
			}

			return queue;
		}
	} // namespace

	auto Deferred_renderer_factory::_rank_device(vk::PhysicalDevice gpu, util::maybe<std::uint32_t> gqueue)
	        -> int
	{
		auto properties = gpu.getProperties();
		auto features   = gpu.getFeatures();

		auto score = 0;

		if(properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
			score += 200;
		} else if(properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
			score += 100;
		}

		if(!features.geometryShader) {
			return std::numeric_limits<int>::min();
		}

		if(gqueue.is_nothing()) {
			return std::numeric_limits<int>::min();
		}

		if(find_compute_queue(gpu).is_some()) {
			score -= 500;
		}

		for(auto& pass : _pass_factories) {
			score = pass->rank_device(gpu, gqueue, score);
		}

		return score;
	}

	auto Deferred_renderer_factory::_init_device(vk::PhysicalDevice gpu, util::maybe<std::uint32_t> gqueue)
	        -> graphic::Device_create_info
	{
		auto ret_val = Device_create_info{};

		MIRRAGE_INVARIANT(gqueue.is_some(), "No useable queue family");
		ret_val.queue_families.emplace("draw"_strid, Queue_create_info{gqueue.get_or_throw()});

		ret_val.queue_families.emplace("compute"_strid,
		                               Queue_create_info{find_compute_queue(gpu).get_or_throw(
		                                       "The device doesn't support compute.")});

		auto supported_features = gpu.getFeatures();
		MIRRAGE_INVARIANT(supported_features.samplerAnisotropy,
		                  "Anisotropic filtering is not supported by device!");
		ret_val.features.samplerAnisotropy = true;

		MIRRAGE_INVARIANT(supported_features.textureCompressionBC,
		                  "BC texture compression is not supported by device!");
		ret_val.features.textureCompressionBC = true;

		for(auto& pass : _pass_factories) {
			pass->configure_device(gpu, gqueue, ret_val);
		}

		return ret_val;
	}

	auto Deferred_renderer_factory::_aquire_next_image() -> std::size_t
	{
		if(_aquired_swapchain_image.is_some())
			return _aquired_swapchain_image.get_or_throw();

		_aquired_swapchain_image = _swapchain.acquireNextImage(*_image_acquired, {});

		return _aquired_swapchain_image.get_or_throw();
	}

	void Deferred_renderer_factory::queue_commands(vk::CommandBuffer cmd)
	{
		_queued_commands.emplace_back(cmd);
	}
	auto Deferred_renderer_factory::queue_temporary_command_buffer() -> vk::CommandBuffer
	{
		auto cb = *_device->destroy_after_frame(std::move(_command_buffer_pool.create_primary()[0]));
		queue_commands(cb);

		return cb;
	}

	auto Deferred_renderer_factory::create_compute_command_buffer() -> vk::UniqueCommandBuffer
	{
		return std::move(_compute_command_buffer_pool.create_primary()[0]);
	}
} // namespace mirrage::renderer
