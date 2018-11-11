#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Blit_pass_factory;

	class Blit_pass : public Render_pass {
	  public:
		using Factory = Blit_pass_factory;

		Blit_pass(Deferred_renderer&, graphic::Render_target_2D& src);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Blit"; }

	  private:
		Deferred_renderer&                   _renderer;
		graphic::Texture_2D&                 _src;
		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		graphic::DescriptorSet               _descriptor_set;
		std::vector<graphic::Framebuffer>    _framebuffers;
		graphic::Render_pass                 _render_pass;
	};

	class Blit_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override { return render_pass_id_of<Blit_pass_factory>(); }

		auto create_pass(Deferred_renderer&,
		                 util::maybe<ecs::Entity_manager&>,
		                 Engine&,
		                 bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue, int current_score)
		        -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
