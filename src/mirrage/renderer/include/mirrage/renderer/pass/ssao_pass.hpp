#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Ssao_pass : public Pass {
	  public:
		Ssao_pass(Deferred_renderer&);

		void update(util::Time dt) override;
		void draw(vk::CommandBuffer&,
		          Command_buffer_source&,
		          vk::DescriptorSet global_uniform_set,
		          std::size_t       swapchain_image) override;

		auto name() const noexcept -> const char* override { return "SAO"; }

	  private:
		Deferred_renderer&        _renderer;
		vk::Format                _ao_format;
		graphic::Render_target_2D _ao_result_buffer;
		graphic::Render_target_2D _blur_buffer;
		graphic::Framebuffer      _ao_result_framebuffer;
		graphic::Framebuffer      _blur_framebuffer;
		graphic::Framebuffer      _ao_result_blur_framebuffer;

		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		graphic::Render_pass                 _ssao_render_pass;
		vk::UniqueDescriptorSet              _ssao_descriptor_set;
		graphic::Render_pass                 _blur_render_pass;
		vk::UniqueDescriptorSet              _blur_descriptor_set_horizontal;
		vk::UniqueDescriptorSet              _blur_descriptor_set_vertical;
	};

	class Ssao_pass_factory : public Pass_factory {
	  public:
		auto create_pass(Deferred_renderer&,
		                 ecs::Entity_manager&,
		                 util::maybe<Meta_system&>,
		                 bool& write_first_pp_buffer) -> std::unique_ptr<Pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue, int current_score)
		        -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};
}
