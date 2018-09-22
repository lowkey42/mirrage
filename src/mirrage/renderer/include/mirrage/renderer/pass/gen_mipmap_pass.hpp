#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	/**
	 * @brief Generates mipmaps for depth and normal (mat_data) buffer
	 */
	class Gen_mipmap_pass : public Render_pass {
	  public:
		Gen_mipmap_pass(Deferred_renderer&);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Gen. Mipmaps"; }

	  private:
		Deferred_renderer&                   _renderer;
		std::uint32_t                        _mip_count;
		vk::UniqueSampler                    _gbuffer_sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;

		// used to generate mipmaps of layer N, based on layer N-1
		std::vector<graphic::DescriptorSet> _descriptor_sets;
		std::vector<graphic::Framebuffer>   _mipmap_gen_framebuffers;
		graphic::Render_pass                _mipmap_gen_renderpass;
	};

	class Gen_mipmap_pass_factory : public Render_pass_factory {
	  public:
		auto create_pass(Deferred_renderer&, ecs::Entity_manager&, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
