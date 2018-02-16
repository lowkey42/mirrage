#pragma once

#include <vulkan/vulkan.hpp>

#include <initializer_list>
#include <memory>
#include <mutex>
#include <vector>


namespace mirrage::graphic {
	// fwd:
	class Device;
	class Descriptor_pool;


	using Descriptor_pool_chunk_index = std::int_fast32_t;

	class DescriptorSet {
	  public:
		DescriptorSet() = default;
		DescriptorSet(DescriptorSet&&) noexcept;
		DescriptorSet& operator=(DescriptorSet&&) noexcept;
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		auto get() const noexcept { return _set; }
		auto get_ptr() const noexcept { return &_set; }
		auto operator*() const noexcept { return get(); }

		explicit operator bool() const noexcept { return get(); }

	  private:
		friend class Descriptor_pool;

		DescriptorSet(Descriptor_pool*,
		              Descriptor_pool_chunk_index,
		              vk::DescriptorSet,
		              std::uint32_t reserved_bindings);

		Descriptor_pool*            _pool = nullptr;
		Descriptor_pool_chunk_index _chunk;
		vk::DescriptorSet           _set;
		std::uint32_t               _reserved_bindings;
	};

	class Descriptor_pool {
	  public:
		// Allocates a descriptor from the pool
		// \param bindings: The estimated number of bindings required by the layout
		auto create_descriptor(vk::DescriptorSetLayout, std::uint32_t bindings) -> DescriptorSet;

	  private:
		friend class Device;
		friend class DescriptorSet;

		vk::Device                            _device;
		std::uint32_t                         _chunk_size;
		std::vector<vk::DescriptorPoolSize>   _pool_sizes;
		std::vector<vk::UniqueDescriptorPool> _chunks;
		std::vector<std::uint32_t>            _chunks_free_count;
		mutable std::mutex                    _mutex;

		Descriptor_pool(vk::Device                                device,
		                std::uint32_t                             chunk_size,
		                std::initializer_list<vk::DescriptorType> types);

		Descriptor_pool(const Descriptor_pool&) = delete;
		Descriptor_pool(Descriptor_pool&&)      = delete;
		Descriptor_pool& operator=(const Descriptor_pool&) = delete;
		Descriptor_pool& operator=(Descriptor_pool&&) = delete;

		auto _create_descriptor_pool() -> vk::DescriptorPool;
		void _free_descriptor_set(vk::DescriptorSet&          set,
		                          Descriptor_pool_chunk_index chunk,
		                          std::uint32_t               reserved_bindings);
	};

	class Image_descriptor_set_layout {
	  public:
		Image_descriptor_set_layout(graphic::Device& device,
		                            vk::Sampler      sampler,
		                            std::uint32_t    image_number,
		                            vk::ShaderStageFlags = vk::ShaderStageFlagBits::eFragment);

		auto layout() const noexcept { return *_layout; }
		auto operator*() const noexcept { return *_layout; }

		auto create_set(Descriptor_pool& pool, std::initializer_list<vk::ImageView> images) -> DescriptorSet;

		void update_set(vk::DescriptorSet, std::initializer_list<vk::ImageView>);

	  private:
		graphic::Device&              _device;
		vk::Sampler                   _sampler;
		std::uint32_t                 _image_number;
		vk::UniqueDescriptorSetLayout _layout;
	};

} // namespace mirrage::graphic
