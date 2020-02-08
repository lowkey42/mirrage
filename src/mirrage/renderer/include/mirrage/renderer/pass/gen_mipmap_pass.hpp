#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Gen_mipmap_pass_factory;

	/**
	 * @brief Generates mipmaps for depth and normal (mat_data) buffer
	 */
	class Gen_mipmap_pass : public Render_pass {
	  public:
		using Factory = Gen_mipmap_pass_factory;

		Gen_mipmap_pass(Deferred_renderer&);


		void update(util::Time dt) override;
		void post_draw(Frame_data&);

		auto name() const noexcept -> const char* override { return "Gen. Mipmaps"; }

	  private:
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
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Gen_mipmap_pass_factory>();
		}

		auto create_pass(Deferred_renderer&,
		                 std::shared_ptr<void>,
		                 util::maybe<ecs::Entity_manager&>,
		                 Engine&,
		                 bool&) -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
