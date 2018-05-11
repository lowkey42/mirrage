#pragma once

#include <mirrage/renderer/camera_comp.hpp>
#include <mirrage/renderer/gbuffer.hpp>
#include <mirrage/renderer/model.hpp>

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/profiler.hpp>

#include <mirrage/utils/math.hpp>

#include <vulkan/vulkan.hpp>


namespace mirrage {
	namespace ecs {
		class Entity_manager;
	}
	class Meta_system;
} // namespace mirrage

namespace mirrage::renderer {

	class Deferred_renderer_factory;
	class Deferred_renderer;


	struct Renderer_settings {
		int shadowmap_resolution = 2048;
		int shadow_quality       = 99; // 0 = lowest

		bool  gi                        = true;
		bool  gi_highres                = true;
		int   gi_diffuse_mip_level      = 1;
		int   gi_min_mip_level          = 0;
		int   gi_samples                = 64;
		int   gi_lowres_samples         = 512;
		bool  gi_jitter_samples         = false;
		int   gi_low_quality_mip_levels = 0;
		float exposure_override         = 0.f;
		bool  histogram_adjustment      = true;
		bool  histogram_trim            = true;

		bool ssao  = true;
		bool bloom = true;

		float background_intensity = 0.f;

		bool dynamic_shadows = false;
		bool debug_disect    = false;
		int  debug_gi_layer  = -1;
	};

#ifdef sf2_structDef
	sf2_structDef(
	        Renderer_settings, shadowmap_resolution, shadow_quality, gi, dynamic_shadows, debug_gi_layer);
#endif

	struct Global_uniforms {
		glm::mat4 view_proj_mat;
		glm::mat4 view_mat;
		glm::mat4 proj_mat;
		glm::mat4 inv_view_mat;
		glm::mat4 inv_proj_mat;
		glm::vec4 eye_pos;
		glm::vec4 proj_planes; //< near, far, fov horizontal, fov vertical
		glm::vec4 time;        //< time, sin(time), delta_time
		glm::vec4 proj_info;
	};

	using Command_buffer_source = std::function<vk::CommandBuffer()>;

	class Pass {
	  public:
		virtual ~Pass() = default;

		virtual void update(util::Time dt)             = 0;
		virtual void draw(vk::CommandBuffer&,
		                  Command_buffer_source&,
		                  vk::DescriptorSet global_uniform_set,
		                  std::size_t       swapchain_image) = 0;

		virtual void shrink_to_fit() {}
		virtual void process_camera(Camera_state&) {} //< allows passes to modify the current camera

		virtual auto name() const noexcept -> const char* = 0;
	};

	class Pass_factory {
	  public:
		virtual ~Pass_factory() = default;

		virtual auto create_pass(Deferred_renderer&,
		                         ecs::Entity_manager&,
		                         util::maybe<Meta_system&>,
		                         bool& write_first_pp_buffer) -> std::unique_ptr<Pass> = 0;

		virtual auto rank_device(vk::PhysicalDevice,
		                         util::maybe<std::uint32_t> graphics_queue,
		                         int                        current_score) -> int
		{
			return current_score;
		}

		virtual void configure_device(vk::PhysicalDevice,
		                              util::maybe<std::uint32_t> graphics_queue,
		                              graphic::Device_create_info&)
		{
		}
	};

	template <class T, class... Args>
	auto make_pass_factory(Args&&... args)
	{
		return std::unique_ptr<Pass_factory>(new T(std::forward<Args>(args)...));
	}


	// shared among all Deferred_renderers in all screens
	class Deferred_renderer_factory {
	  public:
		Deferred_renderer_factory(graphic::Context&,
		                          graphic::Window&      window,
		                          asset::Asset_manager& assets,
		                          std::vector<std::unique_ptr<Pass_factory>>);
		~Deferred_renderer_factory();

		auto create_renderer(ecs::Entity_manager&, util::maybe<Meta_system&>)
		        -> std::unique_ptr<Deferred_renderer>;

		void queue_commands(vk::CommandBuffer);
		auto queue_temporary_command_buffer() -> vk::CommandBuffer;

		auto create_compute_command_buffer() -> vk::UniqueCommandBuffer;

		auto model_material_sampler() const noexcept { return *_model_material_sampler; }
		auto model_descriptor_set_layout() const noexcept { return *_model_desc_set_layout; }

		auto compute_queue() const noexcept { return _compute_queue; }

		void finish_frame();

		auto settings() const -> auto& { return *_settings; }
		void settings(const Renderer_settings& s, bool apply = true);
		void save_settings();

	  private:
		friend class Deferred_renderer;
		struct Asset_loaders;
		using Pass_factories = std::vector<std::unique_ptr<Pass_factory>>;
		using Settings_ptr   = asset::Ptr<Renderer_settings>;

		asset::Asset_manager&           _assets;
		Settings_ptr                    _settings;
		Pass_factories                  _pass_factories;
		graphic::Window&                _window;
		graphic::Device_ptr             _device;
		graphic::Swapchain&             _swapchain;
		std::uint32_t                   _draw_queue_family;
		std::uint32_t                   _compute_queue_family;
		vk::Queue                       _draw_queue;
		vk::Queue                       _compute_queue;
		vk::UniqueSemaphore             _image_acquired;
		vk::UniqueSemaphore             _image_presented;
		graphic::Command_buffer_pool    _command_buffer_pool;
		graphic::Command_buffer_pool    _compute_command_buffer_pool;
		util::maybe<std::size_t>        _aquired_swapchain_image;
		std::vector<vk::CommandBuffer>  _queued_commands;
		std::vector<Deferred_renderer*> _renderer_instances;
		bool                            _recreation_pending = false;
		vk::UniqueSampler               _model_material_sampler;
		vk::UniqueDescriptorSetLayout   _model_desc_set_layout;
		std::unique_ptr<Asset_loaders>  _asset_loaders;

		void _present();
		auto _rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> gqueue) -> int;
		auto _init_device(vk::PhysicalDevice, util::maybe<std::uint32_t> gqueue)
		        -> graphic::Device_create_info;

		auto _aquire_next_image() -> std::size_t;
	};

	class Deferred_renderer {
	  public:
		Deferred_renderer(Deferred_renderer_factory&,
		                  std::vector<std::unique_ptr<Pass_factory>>&,
		                  ecs::Entity_manager&,
		                  util::maybe<Meta_system&>);
		Deferred_renderer(const Deferred_renderer&) = delete;
		auto operator=(const Deferred_renderer&) -> Deferred_renderer& = delete;
		~Deferred_renderer();

		void recreate();

		template <class T>
		auto find_pass() -> util::tracking_ptr<T>
		{
			auto pass = std::find_if(
			        _passes.begin(), _passes.end(), [](auto& p) { return dynamic_cast<T*>(&*p) != nullptr; });

			return pass != _passes.end() ? util::tracking_ptr<T>(pass->create_ptr())
			                             : util::tracking_ptr<T>{};
		}

		void update(util::Time dt);
		void draw();

		void shrink_to_fit();

		auto gbuffer() noexcept -> auto& { return *_gbuffer; }
		auto gbuffer() const noexcept -> auto& { return *_gbuffer; }
		auto global_uniforms() const noexcept -> auto& { return _global_uniforms; }
		auto global_uniforms_layout() const noexcept { return *_global_uniform_descriptor_set_layout; }

		auto asset_manager() noexcept -> auto& { return _factory->_assets; }
		auto device() noexcept -> auto& { return *_factory->_device; }
		auto window() noexcept -> auto& { return _factory->_window; }
		auto swapchain() noexcept -> auto& { return _factory->_swapchain; }
		auto queue_family() const noexcept { return _factory->_draw_queue_family; }
		auto compute_queue_family() const noexcept { return _factory->_compute_queue_family; }

		auto compute_queue() const noexcept { return _factory->compute_queue(); }
		auto create_compute_command_buffer() { return _factory->create_compute_command_buffer(); }

		auto create_descriptor_set(vk::DescriptorSetLayout, std::uint32_t bindings) -> graphic::DescriptorSet;
		auto descriptor_pool() noexcept -> auto& { return _descriptor_set_pool; }

		auto noise_descriptor_set_layout() const noexcept { return *_noise_descriptor_set_layout; }
		auto noise_descriptor_set() const noexcept { return *_noise_descriptor_set; }

		auto model_material_sampler() const noexcept { return _factory->model_material_sampler(); }
		auto model_descriptor_set_layout() const noexcept { return _factory->model_descriptor_set_layout(); }

		auto active_camera() noexcept -> util::maybe<Camera_state&>;

		auto settings() const -> auto& { return _factory->settings(); }
		void save_settings() { _factory->save_settings(); }
		void settings(const Renderer_settings& s, bool apply = true) { _factory->settings(s, apply); }


		auto profiler() const noexcept -> auto& { return _profiler; }
		auto profiler() noexcept -> auto& { return _profiler; }

	  private:
		friend class Deferred_renderer_factory;

		Deferred_renderer_factory* _factory;
		ecs::Entity_manager*       _entity_manager;
		util::maybe<Meta_system&>  _meta_system;
		graphic::Descriptor_pool   _descriptor_set_pool;

		std::unique_ptr<GBuffer> _gbuffer;
		Global_uniforms          _global_uniforms;
		graphic::Profiler        _profiler;
		float                    _time_acc      = 0.f;
		float                    _delta_time    = 0.f;
		std::uint32_t            _frame_counter = 0;

		vk::UniqueDescriptorSetLayout _global_uniform_descriptor_set_layout;
		graphic::DescriptorSet        _global_uniform_descriptor_set;
		graphic::Dynamic_buffer       _global_uniform_buffer;

		graphic::Texture_ptr                 _blue_noise;
		vk::UniqueSampler                    _noise_sampler;
		graphic::Image_descriptor_set_layout _noise_descriptor_set_layout;
		graphic::DescriptorSet               _noise_descriptor_set;

		std::vector<util::trackable<Pass>> _passes;

		Camera_comp::Pool*        _cameras;
		util::maybe<Camera_state> _active_camera;

		void _write_global_uniform_descriptor_set();
		void _update_global_uniforms(vk::CommandBuffer, const Camera_state& camera);
	};
} // namespace mirrage::renderer
