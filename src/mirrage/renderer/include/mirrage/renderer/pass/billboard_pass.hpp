#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Billboard_pass_factory;

	class Billboard_pass : public Render_pass {
	  public:
		using Factory = Billboard_pass_factory;

		Billboard_pass(Deferred_renderer&, ecs::Entity_manager&, graphic::Render_target_2D& target);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Billboard"; }

	  private:
		Deferred_renderer&   _renderer;
		ecs::Entity_manager& _entities;
		graphic::Framebuffer _framebuffer;
		graphic::Render_pass _render_pass;
	};

	class Billboard_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Billboard_pass_factory>();
		}

		auto create_pass(Deferred_renderer&,
		                 std::shared_ptr<void>,
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
