#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	struct Taa_constants {
		glm::mat4 reprojection{};
		glm::mat4 fov_reprojection{}; // fov_reprojection[3].xy = offset
	};

	class Taa_pass_factory;

	class Taa_pass : public Render_pass {
	  public:
		using Factory = Taa_pass_factory;

		Taa_pass(Deferred_renderer&, graphic::Render_target_2D& write, graphic::Texture_2D& read);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		void process_camera(Camera_state&) override;

		auto name() const noexcept -> const char* override { return "TAA"; }

	  private:
		Deferred_renderer&                   _renderer;
		graphic::Framebuffer                 _framebuffer;
		vk::UniqueSampler                    _sampler;
		graphic::Image_descriptor_set_layout _descriptor_set_layout;
		graphic::Render_pass                 _render_pass;
		graphic::Texture_2D&                 _read_frame;
		graphic::Render_target_2D&           _write_frame;
		graphic::Render_target_2D            _prev_frame;
		graphic::DescriptorSet               _descriptor_set;

		bool          _first_frame   = true;
		bool          _render_into_a = true;
		std::size_t   _offset_idx    = 0;
		float         _time_acc      = 0.f;
		glm::mat4     _prev_view_proj;
		Taa_constants _constants;

		auto _calc_offset(const Camera_state&) const -> glm::vec2;
	};

	class Taa_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override { return render_pass_id_of<Taa_pass_factory>(); }

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
