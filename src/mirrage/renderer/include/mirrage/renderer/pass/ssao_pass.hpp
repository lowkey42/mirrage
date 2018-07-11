#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Ssao_pass : public Render_pass {
	  public:
		Ssao_pass(Deferred_renderer&);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

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
		graphic::DescriptorSet               _ssao_descriptor_set;
		graphic::Render_pass                 _blur_render_pass;
		graphic::DescriptorSet               _blur_descriptor_set_horizontal;
		graphic::DescriptorSet               _blur_descriptor_set_vertical;
	};

	class Ssao_pass_factory : public Render_pass_factory {
	  public:
		auto create_pass(Deferred_renderer&, ecs::Entity_manager&, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
