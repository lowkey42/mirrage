#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/graphic/streamed_buffer.hpp>


namespace mirrage::renderer {

	class Debug_draw_pass_factory;

	class Debug_draw_pass : public Render_pass {
	  public:
		using Factory = Debug_draw_pass_factory;

		Debug_draw_pass(Deferred_renderer&, graphic::Render_target_2D& src);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Debug_draw"; }

	  private:
		Deferred_renderer&       _renderer;
		graphic::Streamed_buffer _vertices;
		graphic::Framebuffer     _framebuffer;
		graphic::Render_pass     _render_pass;
	};

	class Debug_draw_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Debug_draw_pass_factory>();
		}

		auto create_pass(Deferred_renderer&,
		                 std::shared_ptr<void>,
		                 util::maybe<ecs::Entity_manager&>,
		                 Engine&,
		                 bool&) -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue, int current_score)
		        -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t> graphics_queue,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
