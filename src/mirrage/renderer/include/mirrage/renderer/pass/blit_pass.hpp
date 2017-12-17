#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Blit_pass : public Pass {
	  public:
		Blit_pass(Deferred_renderer&,
		          ecs::Entity_manager&,
		          util::maybe<Meta_system&>,
		          graphic::Texture_2D& src);


		void update(util::Time dt) override;
		void draw(vk::CommandBuffer&,
		          Command_buffer_source&,
		          vk::DescriptorSet global_uniform_set,
		          std::size_t       swapchain_image) override;

		auto name() const noexcept -> const char* override { return "Blit"; }

	  private:
		Deferred_renderer&                   _renderer;
		graphic::Texture_2D&                 _src;
		graphic::Texture_ptr                 _blue_noise;
		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		vk::UniqueDescriptorSet              _descriptor_set;
		std::vector<graphic::Framebuffer>    _framebuffers;
		graphic::Render_pass                 _render_pass;
		const bool                           _tone_mapping_enabled;
		const bool                           _bloom_enabled;
	};

	class Blit_pass_factory : public Pass_factory {
	  public:
		auto create_pass(Deferred_renderer&,
		                 ecs::Entity_manager&,
		                 util::maybe<Meta_system&>,
		                 bool& write_first_pp_buffer) -> std::unique_ptr<Pass> override;

		auto rank_device(vk::PhysicalDevice,
		                 util::maybe<std::uint32_t> graphics_queue,
		                 int                        current_score) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
