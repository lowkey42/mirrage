#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	class Clear_pass_factory;

	class Clear_pass : public Render_pass {
	  public:
		using Factory = Clear_pass_factory;

		Clear_pass(Deferred_renderer&);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Clear"; }

	  private:
		Deferred_renderer& _renderer;
	};

	class Clear_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Clear_pass_factory>();
		}

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
