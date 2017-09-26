#include <mirrage/graphic/device.hpp>

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/window.hpp>

#include <mirrage/asset/asset_manager.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	namespace {
		constexpr auto max_pipeline_cache_count = 3;

		auto locate_matching_pipeline_cache(vk::PhysicalDevice gpu, asset::Asset_manager& assets) {
			auto properties = gpu.getProperties();

			auto dev_str = std::to_string(properties.vendorID) + "_" + std::to_string(properties.deviceID);

			auto caches = assets.list("pl_cache"_strid);
			for(auto& cache : caches) {
				if(util::ends_with(cache.name(), dev_str)) {
					return cache;
				}
			}

			DEBUG("No pipeline cache found for device, creating new one: dev_" + dev_str);

			if(caches.size() >= max_pipeline_cache_count) {
				DEBUG("More than " << max_pipeline_cache_count
				                   << " pipeline cahces. Deleting oldest caches.");

				std::sort(std::begin(caches), std::end(caches), [&](auto& lhs, auto& rhs) {
					return assets.last_modified(lhs).get_or_other(-1)
					       > assets.last_modified(rhs).get_or_other(-1);
				});

				std::for_each(
				        std::begin(caches) + max_pipeline_cache_count - 1, std::end(caches), [&](auto& aid) {
					        if(!assets.try_delete(aid)) {
						        WARN("Unable to delete outdated pipeline cache: " + aid.str());
					        }
					    });
			}

			return asset::AID{"pl_cache"_strid, "dev_" + dev_str};
		}

		auto load_pipeline_cache(const vk::Device&     device,
		                         asset::Asset_manager& assets,
		                         const asset::AID&     id) {
			auto in = assets.load_raw(id);
			if(in.is_nothing()) {
				return device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo{});

			} else {
				auto data = in.get_or_throw().bytes();
				return device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo{
				        vk::PipelineCacheCreateFlags{}, data.size(), data.data()});
			}
		}

		void store_pipeline_cache(const vk::Device&     device,
		                          asset::Asset_manager& assets,
		                          const asset::AID&     id,
		                          vk::PipelineCache     cache) {
			auto out = assets.save_raw(id);

			auto data = device.getPipelineCacheData(cache);

			out.write(reinterpret_cast<const char*>(data.data()), data.size());
		}
	}

	Swapchain::Swapchain(const vk::Device&          dev,
	                     vk::PhysicalDevice         gpu,
	                     Window&                    window,
	                     vk::SwapchainCreateInfoKHR info)
	  : Window_modification_handler(window)
	  , _device(dev)
	  , _gpu(gpu)
	  , _window(window)
	  , _info(info)
	  , _swapchain(dev.createSwapchainKHRUnique(info))
	  , _images(dev.getSwapchainImagesKHR(*_swapchain))
	  , _image_width(info.imageExtent.width)
	  , _image_height(info.imageExtent.height)
	  , _image_format(info.imageFormat) {

		_create_image_views();
	}

	void Swapchain::_create_image_views() {
		DEBUG("Created swapchain with " << _images.size() << " images (min=" << _info.minImageCount << ")");

		_image_views.clear();
		_image_views.reserve(_images.size());
		for(auto& image : _images) {
			auto ivc = vk::ImageViewCreateInfo{};
			ivc.setImage(image);
			ivc.setViewType(vk::ImageViewType::e2D);
			ivc.setFormat(_info.imageFormat);
			ivc.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
			_image_views.emplace_back(_device.createImageViewUnique(ivc));
		}
	}

	auto Swapchain::acquireNextImage(vk::Semaphore s, vk::Fence f) const -> std::size_t {
		return _device.acquireNextImageKHR(*_swapchain, std::numeric_limits<std::uint64_t>::max(), s, f).value;
	}
	bool Swapchain::present(vk::Queue& q, std::size_t img_index, vk::Semaphore s) {
		auto img_index_vk = gsl::narrow<uint32_t>(img_index);

		auto info   = vk::PresentInfoKHR{s ? 1u : 0u, &s, 1, &*_swapchain, &img_index_vk};
		auto result = vkQueuePresentKHR(VkQueue(q), reinterpret_cast<VkPresentInfoKHR*>(&info));

		_window.on_present();

		if(result != VK_SUCCESS || _recreate_pending) {
			_recreate_pending = false;
			_device.waitIdle();

			auto capabilities = _gpu.getSurfaceCapabilitiesKHR(_window.surface());
			DEBUG("Extends: " << capabilities.currentExtent.width << ", "
			                  << capabilities.currentExtent.height);

			_image_width  = _window.width();
			_image_height = _window.height();

			_info.oldSwapchain       = *_swapchain;
			_info.imageExtent.width  = _image_width;
			_info.imageExtent.height = _image_height;

			_swapchain = _device.createSwapchainKHRUnique(_info);
			_images    = _device.getSwapchainImagesKHR(*_swapchain);

			_create_image_views();

			INFO("Swapchain recreated");

			return true;
		}

		return false;
	}
	void Swapchain::recreate() { _recreate_pending = true; }
	void Swapchain::on_window_modified(Window& window) { recreate(); }



	Device::Device(Context&               context,
	               asset::Asset_manager&  assets,
	               vk::UniqueDevice       device,
	               vk::PhysicalDevice     gpu,
	               Queue_tag              transfer_queue,
	               Queue_family_mapping   queue_mapping,
	               Swapchain_create_infos swapchains,
	               bool                   dedicated_alloc_supported)
	  : util::Registered<Device, Context>(context)
	  , _device(std::move(device))
	  , _gpu(gpu)
	  , _assets(assets)
	  , _gpu_properties(gpu.getProperties())
	  , _pipeline_cache_id(locate_matching_pipeline_cache(_gpu, assets))
	  , _pipeline_cache(load_pipeline_cache(*_device, assets, _pipeline_cache_id))
	  , _queue_family_mappings(std::move(queue_mapping))
	  , _memory_allocator(*_device, _gpu, dedicated_alloc_supported)
	  , _transfer_manager(*this, transfer_queue, 2)
	  , _delete_queue(*this, 2, false)
	  , _depth_format(get_supported_format({vk::Format::eD24UnormS8Uint,
	                                        vk::Format::eD32SfloatS8Uint,
	                                        vk::Format::eD32Sfloat,
	                                        vk::Format::eD16Unorm,
	                                        vk::Format::eD16UnormS8Uint},
	                                       vk::FormatFeatureFlagBits::eDepthStencilAttachment,
	                                       Format_usage::image_optimal)
	                          .get_or_throw("No depth format supported"
	                                        " by device"))
	  , _depth_stencil_format(get_supported_format({vk::Format::eD24UnormS8Uint,
	                                                vk::Format::eD32SfloatS8Uint,
	                                                vk::Format::eD16UnormS8Uint},
	                                               vk::FormatFeatureFlagBits::eDepthStencilAttachment,
	                                               Format_usage::image_optimal)
	                                  .get_or_throw("No depth-stencil format "
	                                                "supported by device"))

	  , _r_format(get_supported_format({vk::Format::eR8Unorm},
	                                   vk::FormatFeatureFlagBits::eBlitDst
	                                           | vk::FormatFeatureFlagBits::eBlitSrc
	                                           | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                   Format_usage::image_optimal))
	  , _rg_format(get_supported_format({vk::Format::eR8G8Unorm},
	                                    vk::FormatFeatureFlagBits::eBlitDst
	                                            | vk::FormatFeatureFlagBits::eBlitSrc
	                                            | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                    Format_usage::image_optimal))
	  , _rgb_format(get_supported_format({vk::Format::eR8G8B8Unorm},
	                                     vk::FormatFeatureFlagBits::eBlitDst
	                                             | vk::FormatFeatureFlagBits::eBlitSrc
	                                             | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                     Format_usage::image_optimal))
	  , _rgba_format(get_supported_format({vk::Format::eR8G8B8A8Unorm},
	                                      vk::FormatFeatureFlagBits::eBlitDst
	                                              | vk::FormatFeatureFlagBits::eBlitSrc
	                                              | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                      Format_usage::image_optimal))

	  , _sr_format(get_supported_format({vk::Format::eR8Srgb},
	                                    vk::FormatFeatureFlagBits::eBlitDst
	                                            | vk::FormatFeatureFlagBits::eBlitSrc
	                                            | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                    Format_usage::image_optimal))
	  , _srg_format(get_supported_format({vk::Format::eR8G8Srgb},
	                                     vk::FormatFeatureFlagBits::eBlitDst
	                                             | vk::FormatFeatureFlagBits::eBlitSrc
	                                             | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                     Format_usage::image_optimal))
	  , _srgb_format(get_supported_format({vk::Format::eR8G8B8Srgb},
	                                      vk::FormatFeatureFlagBits::eBlitDst
	                                              | vk::FormatFeatureFlagBits::eBlitSrc
	                                              | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                      Format_usage::image_optimal))
	  , _srgba_format(get_supported_format({vk::Format::eR8G8B8A8Srgb},
	                                       vk::FormatFeatureFlagBits::eBlitDst
	                                               | vk::FormatFeatureFlagBits::eBlitSrc
	                                               | vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	                                       Format_usage::image_optimal)) {

		for(auto& sc_info : swapchains) {
			_swapchains.emplace(
			        sc_info.first,
			        Swapchain{*_device, _gpu, *std::get<0>(sc_info.second), std::get<1>(sc_info.second)});
		}
	}

	Device::~Device() {
		wait_idle();
		backup_caches();

		print_memory_usage(std::cerr);

		_delete_queue.clear();
		_memory_allocator.shrink_to_fit();

		print_memory_usage(std::cerr);
	}
	void Device::print_memory_usage(std::ostream& log) const {
		auto mem_usage = _memory_allocator.usage_statistic();

		log << "Memory usage: " << mem_usage.used << "/" << mem_usage.reserved << "\n";
		log << "  Types:\n";
		for(auto& t : mem_usage.types) {
			log << "    " << t.first << " : " << t.second.used << "/" << t.second.reserved << "\n";
			log << "      "
			    << "temporary : " << t.second.temporary_pool.used << "/" << t.second.temporary_pool.reserved
			    << " in " << t.second.temporary_pool.blocks << " blocks\n";

			log << "      "
			    << "normal    : " << t.second.normal_pool.used << "/" << t.second.normal_pool.reserved
			    << " in " << t.second.normal_pool.blocks << " blocks\n";

			log << "      "
			    << "persistent : " << t.second.persistent_pool.used << "/"
			    << t.second.persistent_pool.reserved << " in " << t.second.persistent_pool.blocks
			    << " blocks\n";
		}

		log << std::endl;
	}

	void Device::backup_caches() {
		store_pipeline_cache(*_device, _assets, _pipeline_cache_id, *_pipeline_cache);
	}

	auto Device::get_queue(Queue_tag tag) -> vk::Queue {
		auto real_familiy = _queue_family_mappings.find(tag);
		INVARIANT(real_familiy != _queue_family_mappings.end(), "Unknown queue family tag: " << tag.str());

		return _device->getQueue(std::get<0>(real_familiy->second), std::get<1>(real_familiy->second));
	}
	auto Device::get_queue_family(Queue_tag tag) -> std::uint32_t {
		auto real_familiy = _queue_family_mappings.find(tag);
		INVARIANT(real_familiy != _queue_family_mappings.end(), "Unknown queue family tag: " << tag.str());

		return std::get<0>(real_familiy->second);
	}

	auto Device::create_render_pass_builder() -> Render_pass_builder {
		return Render_pass_builder{*_device, *_pipeline_cache, _assets};
	}

	auto Device::create_semaphore() -> vk::UniqueSemaphore { return _device->createSemaphoreUnique({}); }

	auto Device::create_buffer(vk::BufferCreateInfo info,
	                           bool                 host_visible,
	                           Memory_lifetime      lifetime,
	                           bool                 dedicated) -> Backed_buffer {
#ifdef VK_NV_dedicated_allocation
		auto dedicated_alloc_info = vk::DedicatedAllocationBufferCreateInfoNV{dedicated};
		if(dedicated) {
			info.pNext = &dedicated_alloc_info;
		}
#endif

		auto buffer = _device->createBufferUnique(info);
		auto mem    = [&] {
			if(dedicated) {
				return _memory_allocator.alloc_dedicated(*buffer, host_visible).get_or_throw();
			}

			auto mem_req = _device->getBufferMemoryRequirements(*buffer);
			return _memory_allocator
			        .alloc(mem_req.size, mem_req.alignment, mem_req.memoryTypeBits, host_visible, lifetime)
			        .get_or_throw();
		}();

		_device->bindBufferMemory(*buffer, mem.memory(), mem.offset());

		return {std::move(buffer), std::move(mem)};
	}

	auto Device::create_image(vk::ImageCreateInfo info,
	                          bool                host_visible,
	                          Memory_lifetime     lifetime,
	                          bool                dedicated) -> Backed_image {
#ifdef VK_NV_dedicated_allocation
		auto dedicated_alloc_info = vk::DedicatedAllocationImageCreateInfoNV{dedicated};
		if(dedicated) {
			info.pNext = &dedicated_alloc_info;
		}
#endif

		auto image = _device->createImageUnique(info);
		auto mem   = [&] {
			if(dedicated) {
				return _memory_allocator.alloc_dedicated(*image, host_visible).get_or_throw();
			}

			auto mem_req = _device->getImageMemoryRequirements(*image);
			return _memory_allocator
			        .alloc(mem_req.size, mem_req.alignment, mem_req.memoryTypeBits, host_visible, lifetime)
			        .get_or_throw();
		}();

		_device->bindImageMemory(*image, mem.memory(), mem.offset());

		return {std::move(image), std::move(mem)};
	}

	auto Device::create_image_view(vk::Image            image,
	                               vk::Format           format,
	                               std::uint32_t        base_mipmap,
	                               std::uint32_t        mipmap_levels,
	                               vk::ImageAspectFlags aspect,
	                               vk::ImageViewType    type,
	                               vk::ComponentMapping mapping) -> vk::UniqueImageView {
		auto range = vk::ImageSubresourceRange{aspect, base_mipmap, mipmap_levels, 0, 1};
		return _device->createImageViewUnique(
		        vk::ImageViewCreateInfo{vk::ImageViewCreateFlags{}, image, type, format, mapping, range});
	}

	auto Device::create_sampler(std::uint32_t          mip_levels,
	                            vk::SamplerAddressMode address_mode,
	                            vk::BorderColor        border_color,
	                            vk::Filter             filter,
	                            vk::SamplerMipmapMode  mipmap_mode,
	                            bool                   anisotropic,
	                            vk::CompareOp          depth_compare_op) -> vk::UniqueSampler {
		auto max_aniso = anisotropic ? 16.f : 0.f;

		return _device->createSamplerUnique(vk::SamplerCreateInfo{
		        vk::SamplerCreateFlags{},
		        filter,
		        filter,
		        mipmap_mode,
		        address_mode,
		        address_mode,
		        address_mode,
		        0.f,
		        anisotropic,
		        max_aniso,
		        depth_compare_op != vk::CompareOp::eAlways,
		        depth_compare_op,
		        0.f,
		        float(mip_levels),
		        border_color,
		});
	}


	auto Device::create_command_buffer_pool(Queue_tag queue_family, bool resetable, bool short_lived)
	        -> Command_buffer_pool {
		auto real_familiy = _queue_family_mappings.find(queue_family);
		INVARIANT(real_familiy != _queue_family_mappings.end(),
		          "Unknown queue family tag: " << queue_family.str());

		auto flags = vk::CommandPoolCreateFlags{};
		if(resetable)
			flags = flags | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

		if(short_lived)
			flags = flags | vk::CommandPoolCreateFlagBits::eTransient;

		return {*_device,
		        _device->createCommandPoolUnique(
		                vk::CommandPoolCreateInfo{flags, std::get<0>(real_familiy->second)})};
	}

	auto Device::create_descriptor_set_layout(gsl::span<const vk::DescriptorSetLayoutBinding> bindings)
	        -> vk::UniqueDescriptorSetLayout {
		return _device->createDescriptorSetLayoutUnique(
		        vk::DescriptorSetLayoutCreateInfo{vk::DescriptorSetLayoutCreateFlags{},
		                                          gsl::narrow<std::uint32_t>(bindings.size()),
		                                          bindings.data()});
	}
	auto Device::create_descriptor_set_layout(const vk::DescriptorSetLayoutBinding& binding)
	        -> vk::UniqueDescriptorSetLayout {
		return _device->createDescriptorSetLayoutUnique(
		        vk::DescriptorSetLayoutCreateInfo{vk::DescriptorSetLayoutCreateFlags{}, 1, &binding});
	}

	auto Device::create_descriptor_pool(std::uint32_t maxSets, std::vector<vk::DescriptorPoolSize> pool_sizes)
	        -> Descriptor_pool {
		return {*_device,
		        _device->createDescriptorPoolUnique(
		                vk::DescriptorPoolCreateInfo{vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		                                             maxSets,
		                                             gsl::narrow<std::uint32_t>(pool_sizes.size()),
		                                             pool_sizes.data()})};
	}

	auto Device::create_fence() -> Fence { return Fence{*_device}; }

	auto Device::get_supported_format(std::initializer_list<vk::Format> formats,
	                                  vk::FormatFeatureFlags            flags,
	                                  Format_usage                      usage) -> util::maybe<vk::Format> {

		for(auto format : formats) {
			auto props    = _gpu.getFormatProperties(format);
			auto features = [&] {
				switch(usage) {
					case Format_usage::buffer: return props.bufferFeatures;
					case Format_usage::image_linear: return props.linearTilingFeatures;
					case Format_usage::image_optimal: return props.optimalTilingFeatures;
				}
				FAIL("Unreachable");
			}();

			if((features & flags) == flags) {
				if(usage == Format_usage::buffer)
					return format;
				/*
				// TODO: getImageFormatProperties always fails
				auto img_usage = vk::ImageUsageFlags{};
				
				if(flags | vk::FormatFeatureFlagBits::eSampledImage ||
				   flags | vk::FormatFeatureFlagBits::eSampledImageFilterLinear)
					img_usage |= vk::ImageUsageFlagBits::eSampled;
				
				if(flags | vk::FormatFeatureFlagBits::eColorAttachment ||
				   flags | vk::FormatFeatureFlagBits::eColorAttachment) {
					img_usage |= vk::ImageUsageFlagBits::eColorAttachment;
					if(img_usage | vk::ImageUsageFlagBits::eSampled)
						img_usage |= vk::ImageUsageFlagBits::eInputAttachment;
				}
				
				if(flags | vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
					img_usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
					if(img_usage | vk::ImageUsageFlagBits::eSampled)
						img_usage |= vk::ImageUsageFlagBits::eInputAttachment;
				}
				
				auto tiling = usage==Format_usage::image_linear ? vk::ImageTiling::eLinear
				                                                : vk::ImageTiling::eOptimal;
				auto img_props = vk::ImageFormatProperties{};
				auto ret_val = _gpu.getImageFormatProperties(format,
				                                             vk::ImageType::e2D,
				                                             tiling,
				                                             img_usage,
				                                             vk::ImageCreateFlags{},
				                                             &img_props);
				
				if(ret_val==vk::Result::eErrorFormatNotSupported)
					continue;
				
				if(img_props.maxArrayLayers<=0)
					continue;
				
				if(img_props.maxMipLevels<=0)
					continue;
				
				if(img_props.maxExtent.depth<=0 ||
				   img_props.maxExtent.height<=0 ||
				   img_props.maxExtent.width<=0)
					continue;
				*/
				return format;
			}
		}

		return util::nothing;
	}
}
