#pragma once

#include <mirrage/utils/maybe.hpp>

#include <vulkan/vulkan.hpp>

#include <memory>
#include <unordered_map>


namespace mirrage::graphic {

	enum class Memory_lifetime {
		temporary, // <= a couple of frames
		normal,    // a complete level or longer
		persistent // (almost) never freed
	};

	class Device_memory {
	  public:
		using Deleter = void(void*, std::uint32_t index, std::uint32_t layer, vk::DeviceMemory memory);

		Device_memory() = default;
		Device_memory(void*         owner,
		              Deleter*      deleter,
		              std::uint32_t index,
		              std::uint32_t layer,
		              vk::DeviceMemory,
		              vk::DeviceSize,
		              char* mapped_addr);

		Device_memory(Device_memory&&) noexcept;
		Device_memory& operator=(Device_memory&&) noexcept;
		~Device_memory();

		auto memory() const noexcept { return _memory; }
		auto offset() const noexcept { return _offset; }

		auto mapped_addr() const noexcept -> util::maybe<char*>
		{
			return _mapped_addr ? _mapped_addr : util::maybe<char*>::nothing();
		}

	  private:
		void*            _owner   = nullptr;
		Deleter*         _deleter = nullptr;
		std::uint32_t    _index   = 0;
		std::uint32_t    _layer   = 0;
		vk::DeviceMemory _memory;
		vk::DeviceSize   _offset      = 0;
		char*            _mapped_addr = nullptr;
	};

	class Device_heap;

	struct Device_memory_pool_usage {
		std::size_t reserved;
		std::size_t used;
		std::size_t blocks;
	};
	struct Device_memory_type_usage {
		std::size_t reserved;
		std::size_t used;

		Device_memory_pool_usage temporary_pool;
		Device_memory_pool_usage normal_pool;
		Device_memory_pool_usage persistent_pool;
	};

	struct Device_memory_usage {
		std::size_t reserved;
		std::size_t used;

		std::unordered_map<std::uint32_t, Device_memory_type_usage> types;
	};

	class Device_memory_allocator {
	  public:
		Device_memory_allocator(const vk::Device&  device,
		                        vk::PhysicalDevice gpu,
		                        bool               dedicated_alloc_supported);
		~Device_memory_allocator();

		auto alloc(std::uint32_t   size,
		           std::uint32_t   alignment,
		           std::uint32_t   type_mask,
		           bool            host_visible,
		           Memory_lifetime lifetime = Memory_lifetime::normal) -> util::maybe<Device_memory>;

		auto alloc_dedicated(vk::Image, bool host_visible) -> util::maybe<Device_memory>;
		auto alloc_dedicated(vk::Buffer, bool host_visible) -> util::maybe<Device_memory>;

		auto shrink_to_fit() -> std::size_t;
		auto usage_statistic() const -> Device_memory_usage;

		auto is_dedicated_allocations_supported() const noexcept
		{
			return _is_dedicated_allocations_supported;
		}

		auto is_unified_memory_architecture() const noexcept { return _is_unified_memory_architecture; }

	  private:
		friend class Device_memory;

		const vk::Device&                         _device;
		bool                                      _is_unified_memory_architecture;
		bool                                      _is_dedicated_allocations_supported;
		std::vector<std::unique_ptr<Device_heap>> _pools;
		std::vector<std::uint32_t>                _host_visible_pools;
		std::vector<std::uint32_t>                _device_local_pools;

		auto alloc_dedicated(vk::Buffer, vk::Image, bool host_visible) -> util::maybe<Device_memory>;
	};

	template <class UniqueHandleType>
	class Memory_backed {
	  public:
		using T = std::remove_reference_t<decltype(std::declval<UniqueHandleType>().get())>;

		Memory_backed() = default;
		Memory_backed(UniqueHandleType&& instance, Device_memory&& memory)
		  : _instance(std::move(instance)), _memory(std::move(memory))
		{
		}

		const T& operator*() const noexcept { return *_instance; }
		const T* operator->() const noexcept { return &*_instance; }

		auto memory() -> auto& { return _memory; }

	  private:
		UniqueHandleType _instance;
		Device_memory    _memory;
	};

	using Backed_buffer = Memory_backed<vk::UniqueBuffer>;
	using Backed_image  = Memory_backed<vk::UniqueImage>;
} // namespace mirrage::graphic
