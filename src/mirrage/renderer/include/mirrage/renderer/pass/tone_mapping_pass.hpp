#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Tone_mapping_pass : public Pass {
	  public:
		Tone_mapping_pass(Deferred_renderer&,
		                  ecs::Entity_manager&,
		                  util::maybe<Meta_system&>,
		                  graphic::Texture_2D& src);


		void update(util::Time dt) override;
		void draw(vk::CommandBuffer&,
		          Command_buffer_source&,
		          vk::DescriptorSet global_uniform_set,
		          std::size_t       swapchain_image) override;

		auto name() const noexcept -> const char* override { return "Tone Mapping"; }

	  private:
		Deferred_renderer&                   _renderer;
		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		vk::Format                           _luminance_format;
		int                                  _first_frame = 4;

		// calculate scene luminance for tone mapping
		graphic::Render_target_2D _luminance_buffer;
		graphic::Framebuffer      _calc_luminance_framebuffer;
		graphic::Render_pass      _calc_luminance_renderpass;
		vk::UniqueDescriptorSet   _calc_luminance_desc_set;

		// calculate and adapt avg luminance over time
		graphic::Render_target_2D _prev_avg_luminance;
		graphic::Render_target_2D _curr_avg_luminance;
		graphic::Framebuffer      _adapt_luminance_framebuffer;
		graphic::Render_pass      _adapt_luminance_renderpass;
		vk::UniqueDescriptorSet   _adapt_luminance_desc_set;
	};

	class Tone_mapping_pass_factory : public Pass_factory {
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
} // namespace mirrage::renderer
