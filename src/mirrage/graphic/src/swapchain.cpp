#include <mirrage/graphic/swapchain.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	Swapchain::Swapchain(const vk::Device&          dev,
	                     vk::PhysicalDevice         gpu,
	                     Window&                    window,
	                     vk::SwapchainCreateInfoKHR info)
	  : Window_modification_handler(window)
	  , _device(&dev)
	  , _gpu(gpu)
	  , _window(&window)
	  , _info(info)
	  , _swapchain(dev.createSwapchainKHRUnique(info))
	  , _images(dev.getSwapchainImagesKHR(*_swapchain))
	  , _image_width(gsl::narrow<int>(info.imageExtent.width))
	  , _image_height(gsl::narrow<int>(info.imageExtent.height))
	  , _image_format(info.imageFormat)
	{

		_create_image_views();
	}

	void Swapchain::_create_image_views()
	{
		LOG(plog::debug) << "Created swapchain with " << _images.size()
		                 << " images (min=" << _info.minImageCount << ")";

		_image_views.clear();
		_image_views.reserve(_images.size());
		for(auto& image : _images) {
			auto ivc = vk::ImageViewCreateInfo{};
			ivc.setImage(image);
			ivc.setViewType(vk::ImageViewType::e2D);
			ivc.setFormat(_info.imageFormat);
			ivc.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
			_image_views.emplace_back(_device->createImageViewUnique(ivc));
		}
	}

	auto Swapchain::acquireNextImage(vk::Semaphore s, vk::Fence f) const -> std::size_t
	{
		return _device->acquireNextImageKHR(*_swapchain, std::numeric_limits<std::uint64_t>::max(), s, f)
		        .value;
	}

	bool Swapchain::present(vk::Queue& q, std::size_t img_index, vk::Semaphore s)
	{
		auto img_index_vk = gsl::narrow<uint32_t>(img_index);

		auto info   = vk::PresentInfoKHR{s ? 1u : 0u, &s, 1, &*_swapchain, &img_index_vk};
		auto result = vkQueuePresentKHR(VkQueue(q), reinterpret_cast<VkPresentInfoKHR*>(&info));

		_window->on_present();

		if(result != VK_SUCCESS || _recreate_pending) {
			_recreate_pending = false;
			_device->waitIdle();

			auto capabilities = _gpu.getSurfaceCapabilitiesKHR(_window->surface());
			LOG(plog::debug) << "Extends: " << capabilities.currentExtent.width << ", "
			                 << capabilities.currentExtent.height;

			_image_width  = _window->width();
			_image_height = _window->height();

			_info.oldSwapchain       = *_swapchain;
			_info.imageExtent.width  = gsl::narrow<std::uint32_t>(_image_width);
			_info.imageExtent.height = gsl::narrow<std::uint32_t>(_image_height);

			_swapchain = _device->createSwapchainKHRUnique(_info);
			_images    = _device->getSwapchainImagesKHR(*_swapchain);

			_create_image_views();

			LOG(plog::info) << "Swapchain recreated";

			return true;
		}

		return false;
	}

	void Swapchain::recreate() { _recreate_pending = true; }
	void Swapchain::on_window_modified(Window&) { recreate(); }

} // namespace mirrage::graphic
