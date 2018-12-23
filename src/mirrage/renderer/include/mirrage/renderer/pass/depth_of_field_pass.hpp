#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Depth_of_field_pass_factory;

	/**
	 * @brief Depth of field effect based on
	 *			http://tuxedolabs.blogspot.com/2018/05/bokeh-depth-of-field-in-single-pass.html
	 */
	class Depth_of_field_pass : public Render_pass {
	  public:
		using Factory = Depth_of_field_pass_factory;

		Depth_of_field_pass(Deferred_renderer&,
		                    graphic::Render_target_2D& src,
		                    graphic::Render_target_2D& target);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Depth of Fields"; }

	  private:
		Deferred_renderer&                   _renderer;
		graphic::Render_target_2D&           _src;
		graphic::Render_target_2D&           _target;
		vk::UniqueSampler                    _gbuffer_sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;

		graphic::DescriptorSet _coc_descriptor_set;
		graphic::Framebuffer   _coc_framebuffer;
		graphic::Render_pass   _coc_renderpass;

		graphic::DescriptorSet _dof_descriptor_set;
		graphic::Framebuffer   _dof_framebuffer;
		graphic::Render_pass   _dof_renderpass;

		graphic::DescriptorSet _apply_descriptor_set;
		graphic::Framebuffer   _apply_framebuffer;
		graphic::Render_pass   _apply_renderpass;
	};

	class Depth_of_field_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Depth_of_field_pass_factory>();
		}

		auto create_pass(Deferred_renderer&, util::maybe<ecs::Entity_manager&>, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
