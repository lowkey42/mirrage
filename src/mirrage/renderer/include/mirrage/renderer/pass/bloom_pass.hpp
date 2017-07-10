#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace lux {
namespace renderer {

	class Bloom_pass : public Pass {
		public:
			Bloom_pass(Deferred_renderer&,
			           ecs::Entity_manager&,
			           util::maybe<Meta_system&>,
			           graphic::Render_target_2D& src);


			void update(util::Time dt) override;
			void draw(vk::CommandBuffer&, Command_buffer_source&,
			          vk::DescriptorSet global_uniform_set, std::size_t swapchain_image) override;

			auto name()const noexcept -> const char* override {return "Bloom";}

		private:
			static constexpr auto blur_start_mip_level = 2;
			static constexpr auto blur_mip_levels = 5;

			using Blur_framebuffers = std::array<graphic::Framebuffer, blur_mip_levels-blur_start_mip_level>;

			Deferred_renderer&                   _renderer;
			vk::UniqueSampler                    _sampler;
			graphic::Image_descriptor_set_layout _descriptor_set_layout;

			// copy high intensity colors from color buffer into bloom buffer
			graphic::Render_target_2D _bloom_buffer;
			graphic::Framebuffer      _filter_framebuffer;
			graphic::Render_pass      _filter_renderpass;
			vk::UniqueDescriptorSet   _filter_descriptor_set;

			// blur the bloom buffer
			graphic::Render_target_2D _blur_buffer;
			vk::UniqueImageView       _downsampled_blur_view;
			Blur_framebuffers         _blur_framebuffer_horizontal;
			Blur_framebuffers         _blur_framebuffer_vertical;
			graphic::Render_pass      _blur_renderpass;
			vk::UniqueDescriptorSet   _blur_descriptor_set_horizontal;
			vk::UniqueDescriptorSet   _blur_descriptor_set_vertical;
			vk::UniqueDescriptorSet   _blur_descriptor_set_vertical_final;
	};

	class Bloom_pass_factory : public Pass_factory {
		public:
			auto create_pass(Deferred_renderer&,
			                 ecs::Entity_manager&,
			                 util::maybe<Meta_system&>,
			                 bool& write_first_pp_buffer) -> std::unique_ptr<Pass> override;

			auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
			                 int current_score) -> int override;

			void configure_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
			                      graphic::Device_create_info&) override;
	};

}
}
