#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	/**
	 * @brief Generates mipmaps for depth and normal (mat_data) buffer
	 */
	class Gen_mipmap_pass : public Pass {
	  public:
		Gen_mipmap_pass(Deferred_renderer&);


		void update(util::Time dt) override;
		void draw(vk::CommandBuffer&,
		          Command_buffer_source&,
		          vk::DescriptorSet global_uniform_set,
		          std::size_t       swapchain_image) override;

		auto name() const noexcept -> const char* override { return "Gen. Mipmaps"; }

	  private:
		Deferred_renderer&                   _renderer;
		std::uint32_t                        _mip_count;
		vk::UniqueSampler                    _gbuffer_sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;

		// used to generate mipmaps of layer N, based on layer N-1
		std::vector<vk::UniqueDescriptorSet> _descriptor_sets;
		std::vector<graphic::Framebuffer>    _mipmap_gen_framebuffers;
		graphic::Render_pass                 _mipmap_gen_renderpass;
	};

	class Gen_mipmap_pass_factory : public Pass_factory {
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
