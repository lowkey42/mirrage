#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Bloom_pass : public Render_pass {
	  public:
		Bloom_pass(Deferred_renderer&, graphic::Render_target_2D& src);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Bloom"; }

	  private:
		using Blur_framebuffers = std::vector<graphic::Framebuffer>;

		Deferred_renderer&                   _renderer;
		graphic::Render_target_2D&           _src;
		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		bool                                 _first_frame = true;

		// copy high intensity colors from color buffer into bloom buffer
		graphic::Render_target_2D _bloom_buffer;
		graphic::Framebuffer      _apply_framebuffer;
		graphic::Render_pass      _apply_renderpass;
		graphic::DescriptorSet    _apply_descriptor_set;

		// blur the bloom buffer
		graphic::Render_target_2D _blur_buffer;
		Blur_framebuffers         _blur_framebuffer_horizontal;
		Blur_framebuffers         _blur_framebuffer_vertical;
		graphic::Render_pass      _blur_renderpass;
		graphic::DescriptorSet    _blur_descriptor_set_horizontal;
		graphic::DescriptorSet    _blur_descriptor_set_vertical;

		std::vector<vk::UniqueImageView>    _downsampled_blur_views;
		std::vector<graphic::DescriptorSet> _blur_descriptor_set_vertical_final;
	};

	class Bloom_pass_factory : public Render_pass_factory {
	  public:
		auto create_pass(Deferred_renderer&, ecs::Entity_manager&, Engine&, bool& write_first_pp_buffer)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue, int current_score)
		        -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
