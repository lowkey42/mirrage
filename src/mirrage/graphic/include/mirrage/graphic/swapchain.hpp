#pragma once

#include <mirrage/graphic/window.hpp>

#include <vulkan/vulkan.hpp>

#include <vector>


namespace mirrage::graphic {

	class Swapchain : public Window_modification_handler {
	  public:
		Swapchain() = default;
		Swapchain(const vk::Device& dev, vk::PhysicalDevice, Window&, vk::SwapchainCreateInfoKHR);
		Swapchain(Swapchain&&) = default;
		Swapchain& operator=(Swapchain&&) = default;
		~Swapchain() override             = default;

		auto get_images() const -> auto& { return _image_views; }
		auto acquireNextImage(vk::Semaphore, vk::Fence) const -> std::size_t;

		bool present(vk::Queue&, std::size_t img_index, vk::Semaphore);

		void recreate();

		void on_window_modified(Window&) override;

		auto image_width() const noexcept { return _image_width; }
		auto image_height() const noexcept { return _image_height; }
		auto image_format() const noexcept { return _image_format; }

	  private:
		const vk::Device&                _device;
		vk::PhysicalDevice               _gpu;
		Window&                          _window;
		vk::SwapchainCreateInfoKHR       _info;
		vk::UniqueSwapchainKHR           _swapchain;
		std::vector<vk::Image>           _images;
		std::vector<vk::UniqueImageView> _image_views;
		int                              _image_width;
		int                              _image_height;
		vk::Format                       _image_format;
		bool                             _recreate_pending = false;

		void _create_image_views();
	};

} // namespace mirrage::graphic
