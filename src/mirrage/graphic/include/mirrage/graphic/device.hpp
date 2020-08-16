#pragma once

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/pipeline_cache.hpp>
#include <mirrage/graphic/settings.hpp>
#include <mirrage/graphic/swapchain.hpp>
#include <mirrage/graphic/transfer_manager.hpp>
#include <mirrage/graphic/vk_wrapper.hpp>
#include <mirrage/graphic/window.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/purgatory.hpp>
#include <mirrage/utils/registration.hpp>
#include <mirrage/utils/ring_buffer.hpp>

#include <vulkan/vulkan.hpp>

#include <string>
#include <unordered_map>



namespace mirrage::graphic {

	enum class Format_usage { buffer, image_linear, image_optimal };

	class Device : public util::Registered<Device, Context> {
	  public:
		Device(Context&              context,
		       asset::Asset_manager& assets,
		       vk::UniqueDevice,
		       vk::PhysicalDevice,
		       Queue_tag            transfer_queue,
		       Queue_tag            default_draw_queue,
		       Queue_family_mapping queue_mapping,
		       Swapchain_create_infos,
		       bool                              dedicated_alloc_supported,
		       const vk::PhysicalDeviceFeatures& features);
		~Device();

		auto get_queue(Queue_tag) -> vk::Queue;
		auto get_queue_family(Queue_tag) -> std::uint32_t;

		auto get_single_swapchain() -> auto&
		{
			MIRRAGE_INVARIANT(_swapchains.size() == 1,
			                  "Wrong number of swapchains found: " << _swapchains.size());
			return _swapchains.begin()->second;
		}
		auto get_swapchain(const std::string& id) -> auto&
		{
			auto it = _swapchains.find(id);
			MIRRAGE_INVARIANT(it != _swapchains.end(), "Unknown spawchain: " + id);
			return it->second;
		}

		auto create_render_pass_builder() -> Render_pass_builder;
		auto create_semaphore() -> vk::UniqueSemaphore;
		auto create_fence(bool signaled = false) -> Fence;
		auto create_command_buffer_pool(Queue_tag queue_family,
		                                bool      resetable   = true,
		                                bool      short_lived = false) -> Command_buffer_pool;
		auto create_vk_command_pool(Queue_tag queue_family, bool resetable = true, bool short_lived = false)
		        -> vk::UniqueCommandPool;

		auto create_descriptor_pool(std::int32_t chunk_size, std::initializer_list<vk::DescriptorType> types)
		        -> Descriptor_pool;

		auto create_descriptor_set_layout(gsl::span<const vk::DescriptorSetLayoutBinding> bindings)
		        -> vk::UniqueDescriptorSetLayout;
		auto create_descriptor_set_layout(const vk::DescriptorSetLayoutBinding& binding)
		        -> vk::UniqueDescriptorSetLayout;

		auto create_buffer(vk::BufferCreateInfo,
		                   bool            host_visible,
		                   Memory_lifetime lifetime  = Memory_lifetime::normal,
		                   bool            dedicated = false) -> Backed_buffer;
		auto create_image(vk::ImageCreateInfo,
		                  bool            host_visible = false,
		                  Memory_lifetime lifetime     = Memory_lifetime::normal,
		                  bool            dedicated    = false) -> Backed_image;

		auto create_image_view(vk::Image,
		                       vk::Format,
		                       std::int32_t  base_mipmap,
		                       std::uint32_t mipmap_levels,
		                       vk::ImageAspectFlags = vk::ImageAspectFlagBits::eColor,
		                       vk::ImageViewType    = vk::ImageViewType::e2D,
		                       vk::ComponentMapping = {}) -> vk::UniqueImageView;

		auto create_sampler(std::int32_t max_mip_levels,
		                    vk::SamplerAddressMode         = vk::SamplerAddressMode::eRepeat,
		                    vk::BorderColor                = vk::BorderColor::eIntOpaqueBlack,
		                    vk::Filter                     = vk::Filter::eLinear,
		                    vk::SamplerMipmapMode          = vk::SamplerMipmapMode::eLinear,
		                    bool          anisotropic      = true,
		                    vk::CompareOp depth_compare_op = vk::CompareOp::eAlways) -> vk::UniqueSampler;

		auto get_supported_format(std::initializer_list<vk::Format>,
		                          vk::FormatFeatureFlags,
		                          Format_usage = Format_usage::image_optimal) -> util::maybe<vk::Format>;

		auto get_depth_format() { return _depth_format; }
		auto get_depth_stencil_format() { return _depth_stencil_format; }
		auto get_texture_r_format() { return _r_format; }
		auto get_texture_rg_format() { return _rg_format; }
		auto get_texture_rgb_format() { return _rgb_format; }
		auto get_texture_rgba_format() { return _rgba_format; }
		auto get_texture_sr_format() { return _sr_format; }
		auto get_texture_srg_format() { return _srg_format; }
		auto get_texture_srgb_format() { return _srgb_format; }
		auto get_texture_srgba_format() { return _srgba_format; }

		auto physical_device() const noexcept { return _gpu; }
		auto physical_device_properties() const noexcept { return _gpu_properties; }
		auto physical_device_features() const noexcept { return _features; }

		auto transfer() -> auto& { return _transfer_manager; }

		template <typename T>
		auto destroy_after_frame(T&& obj) -> T&
		{
			return _delete_queue.destroy_later(std::forward<T>(obj));
		}

		auto finish_frame(vk::CommandBuffer transfer_barriers)
		        -> std::tuple<Fence*, util::maybe<vk::Semaphore>>
		{
			auto semaphore = _transfer_manager.next_frame(transfer_barriers);
			return std::make_tuple(&_delete_queue.start_new_frame(), std::move(semaphore));
		}

		void shrink_to_fit() { _memory_allocator.shrink_to_fit(); }
		void print_memory_usage(std::ostream& log) const;

		void backup_caches();

		auto context() -> auto& { return util::Registered<Device, Context>::parent(); }

		void wait_idle(bool clear_delete_queue = false)
		{
			_device->waitIdle();
			if(clear_delete_queue)
				_delete_queue.clear();
		}

		auto is_unified_memory_architecture() const noexcept
		{
			return _memory_allocator.is_unified_memory_architecture();
		}

		auto max_frames_in_flight() const noexcept { return _delete_queue.capacity(); }

		auto vk_device() const noexcept { return &*_device; }
		auto pipeline_cache() const noexcept { return **_pipeline_cache; }

	  private:
		struct Asset_loaders;

		vk::UniqueDevice                           _device;
		vk::PhysicalDevice                         _gpu;
		vk::PhysicalDeviceFeatures                 _features;
		asset::Asset_manager&                      _assets;
		vk::PhysicalDeviceProperties               _gpu_properties;
		std::unordered_map<std::string, Swapchain> _swapchains;
		asset::Ptr<Pipeline_cache>                 _pipeline_cache;
		Queue_family_mapping                       _queue_family_mappings;
		Device_memory_allocator                    _memory_allocator;
		Transfer_manager                           _transfer_manager;
		Delete_queue                               _delete_queue;

		vk::Format              _depth_format;
		vk::Format              _depth_stencil_format;
		util::maybe<vk::Format> _r_format;
		util::maybe<vk::Format> _rg_format;
		util::maybe<vk::Format> _rgb_format;
		util::maybe<vk::Format> _rgba_format;
		util::maybe<vk::Format> _sr_format;
		util::maybe<vk::Format> _srg_format;
		util::maybe<vk::Format> _srgb_format;
		util::maybe<vk::Format> _srgba_format;

		std::unique_ptr<Asset_loaders> _device_specific_asset_loaders;
	};
} // namespace mirrage::graphic
