#pragma once

#include <mirrage/graphic/descriptor_sets.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/purgatory.hpp>
#include <mirrage/utils/ring_buffer.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <vulkan/vulkan.hpp>

#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


namespace mirrage::asset {
	class Asset_manager;
	class AID;
} // namespace mirrage::asset

namespace mirrage::graphic {

	// fwd:
	class Device;
	class Render_pass_builder;
	class Render_pass;
	class Render_pass_builder;
	class Subpass_builder;
	class Framebuffer;
	class Window;


	using Command_buffer = vk::CommandBuffer;

	using Swapchain_create_infos =
	        std::unordered_map<std::string, std::tuple<Window*, vk::SwapchainCreateInfoKHR>>;

	using Queue_tag            = util::Str_id;
	using Queue_family_mapping = std::unordered_map<Queue_tag, std::tuple<std::uint32_t, std::uint32_t>>;


	enum class Image_type {
		single_1d,
		single_2d,
		single_3d,
		array_1d,
		array_2d,
		array_3d,
		cubemap,
		array_cubemap
	};

	struct Image_dimensions {
		Image_dimensions(std::uint32_t width, std::uint32_t height, std::uint32_t depth, std::uint32_t layers)
		  : width(width), height(height), depth(depth), layers(layers)
		{
		}

		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t depth;
		std::uint32_t layers;
	};
	template <Image_type Type>
	struct Image_dimensions_t;

	template <>
	struct Image_dimensions_t<Image_type::single_1d> : Image_dimensions {
		Image_dimensions_t(std::uint32_t width) : Image_dimensions(width, 1, 1, 1) {}
	};
	template <>
	struct Image_dimensions_t<Image_type::single_2d> : Image_dimensions {
		Image_dimensions_t(std::uint32_t width, std::uint32_t height) : Image_dimensions(width, height, 1, 1)
		{
		}
	};
	template <>
	struct Image_dimensions_t<Image_type::array_2d> : Image_dimensions {
		Image_dimensions_t(std::uint32_t width, std::uint32_t height, std::uint32_t layers)
		  : Image_dimensions(width, height, 1, layers)
		{
		}
	};
	template <>
	struct Image_dimensions_t<Image_type::cubemap> : Image_dimensions {
		Image_dimensions_t(std::uint32_t width, std::uint32_t height) : Image_dimensions(width, height, 1, 6)
		{
		}
	};


	class Command_buffer_pool {
	  public:
		Command_buffer_pool(Command_buffer_pool&&) = default;
		Command_buffer_pool& operator=(Command_buffer_pool&&) = default;
		~Command_buffer_pool();

		auto create_primary(std::size_t count = 1) -> std::vector<vk::UniqueCommandBuffer>;
		auto create_secondary(std::size_t count = 1) -> std::vector<vk::UniqueCommandBuffer>;

	  private:
		friend class Device;

		const vk::Device&     _device;
		vk::UniqueCommandPool _pool;

		Command_buffer_pool(const vk::Device& device, vk::UniqueCommandPool pool);
	};


	class Fence {
	  public:
		Fence(Fence&&) = default;
		Fence& operator=(Fence&&) = default;
		~Fence()                  = default;

		auto     vk_fence() const { return *_fence; }
		explicit operator bool() const;
		void     reset();
		void     wait();

	  private:
		friend class Device;

		const vk::Device& _device;
		vk::UniqueFence   _fence;

		Fence(const vk::Device& device, bool signaled);
	};

	extern auto create_fence(Device&) -> Fence;

	template <class T, class Callback = void (*)(T&)>
	class Per_frame_queue {
	  public:
		Per_frame_queue(Device&     device,
		                Callback    callback,
		                std::size_t max_frames   = 4,
		                bool        warn_on_full = true)
		  : _device(device)
		  , _warn_on_full(warn_on_full)
		  , _queues(max_frames, [&] { return create_fence(_device); })
		  , _callback(callback)
		{
		}
		template <typename Factory>
		Per_frame_queue(Device&     device,
		                Callback    callback,
		                Factory&&   factory,
		                std::size_t max_frames   = 4,
		                bool        warn_on_full = true)
		  : _device(device)
		  , _warn_on_full(warn_on_full)
		  , _queues(max_frames,
		            [&] {
			            return Entry{create_fence(_device), factory()};
		            })
		  , _callback(callback)
		{
		}
		~Per_frame_queue() { clear(); }

		auto capacity() const noexcept { return _queues.capacity(); }

		void clear()
		{
			_queues.pop_while([&](auto& e) {
				e.fence.wait();
				_callback(e.data);
				return true;
			});
		}

		auto start_new_frame() -> vk::Fence
		{
			// free unused data from prev frames
			_queues.pop_while([&](auto& e) {
				if(!e.fence)
					return false;

				_callback(e.data);
				return true;
			});

			// advance write frame
			auto& fence = _queues.head().fence;

			while(!_queues.advance_head()) { // no free slot
				LOG_IF(plog::debug, _warn_on_full)
				        << "Delete_queue is full. Increase max_frames or reduce amount of "
				           "frames in flight.";

				_queues.pop([&](auto& e) {
					e.fence.wait();
					_callback(e.data);
				});
			}

			fence.reset();

			return fence.vk_fence();
		}

		auto current() -> T& { return _queues.head().data; }

	  private:
		struct Entry {
			Fence fence;
			T     data;

			Entry(Fence fence) : fence(std::move(fence)) {}
			Entry(Fence fence, T data) : fence(std::move(fence)), data(std::move(data)) {}
		};

		Device&                  _device;
		bool                     _warn_on_full;
		util::ring_buffer<Entry> _queues;
		Callback                 _callback;
	};

	class Delete_queue : public Per_frame_queue<util::purgatory> {
	  public:
		Delete_queue(Device& device, std::size_t max_frames = 4, bool warn_on_full = true)
		  : Per_frame_queue(device, +[](util::purgatory& p) { p.clear(); }, max_frames, warn_on_full)
		{
		}

		template <typename T>
		auto destroy_later(T&& obj) -> T&
		{
			return current().add(std::forward<T>(obj));
		}
	};


	class Framebuffer {
	  public:
		Framebuffer()              = default;
		Framebuffer(Framebuffer&&) = default;
		Framebuffer& operator=(Framebuffer&&) = default;
		~Framebuffer()                        = default;

		void viewport(
		        float x, float y, float width, float height, float min_depth = 0.f, float max_depth = 1.f);

	  private:
		friend class Render_pass;
		friend class Render_pass_builder;

		vk::UniqueFramebuffer _fb;

		Framebuffer(vk::UniqueFramebuffer fb,
		            vk::Viewport          viewport,
		            vk::Rect2D            scissor,
		            std::vector<vk::ClearValue>);
		vk::Viewport                _viewport;
		vk::Rect2D                  _scissor;
		std::vector<vk::ClearValue> _clear_values;
	};


	extern void image_layout_transition(vk::CommandBuffer cb,
	                                    vk::Image         image,
	                                    vk::ImageLayout   src_layout,
	                                    vk::ImageLayout   dst_layout,
	                                    vk::ImageAspectFlags          = vk::ImageAspectFlagBits::eColor,
	                                    std::uint32_t mip_level       = 0,
	                                    std::uint32_t mip_level_count = 1);

	extern void generate_mipmaps(vk::CommandBuffer cb,
	                             vk::Image         image,
	                             vk::ImageLayout   src_layout,
	                             vk::ImageLayout   dst_layout,
	                             std::uint32_t     width,
	                             std::uint32_t     height,
	                             std::uint32_t     mip_count       = 0,
	                             std::uint32_t     start_mip_level = 0,
	                             bool              filter_linear   = true);

	namespace detail {
		class Base_texture;
	}
	extern void blit_texture(vk::CommandBuffer           cb,
	                         const detail::Base_texture& src,
	                         vk::ImageLayout             initial_src_layout,
	                         vk::ImageLayout             final_src_layout,
	                         detail::Base_texture&       dst,
	                         vk::ImageLayout             initial_dst_layout,
	                         vk::ImageLayout             final_dst_layout,
	                         std::int32_t                src_mip = 0,
	                         std::int32_t                dst_mip = 0);

	extern void clear_texture(vk::CommandBuffer           cb,
	                          const detail::Base_texture& img,
	                          util::Rgba                  color,
	                          vk::ImageLayout             initial_layout,
	                          vk::ImageLayout             final_layout,
	                          std::uint32_t               initial_mip_level = 0,
	                          std::uint32_t               mip_levels        = 0);

	extern void clear_texture(vk::CommandBuffer cb,
	                          vk::Image         img,
	                          std::uint32_t     width,
	                          std::uint32_t     height,
	                          util::Rgba        color,
	                          vk::ImageLayout   initial_layout,
	                          vk::ImageLayout   final_layout,
	                          std::uint32_t     initial_mip_level = 0,
	                          std::uint32_t     mip_levels        = 0);

	template <typename P>
	auto find_queue_family(vk::PhysicalDevice& gpu, P&& predicat) -> util::maybe<std::uint32_t>
	{
		auto i = 0u;

		for(auto& queue_family : gpu.getQueueFamilyProperties()) {
			if(queue_family.queueCount > 0 && predicat(queue_family)) {
				return i;
			}

			i++;
		}

		return util::nothing;
	}

} // namespace mirrage::graphic
