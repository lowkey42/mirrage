#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	// TODO: everything

	class Particle_pass_factory;

	/**
	 * @brief Updates particle systems and submits them for drawing
	 * Should be after animation but before other render_passes
	 */
	class Particle_pass : public Render_pass {
	  public:
		using Factory = Particle_pass_factory;

		Particle_pass(Deferred_renderer&, ecs::Entity_manager&);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Particle"; }

	  private:
		Deferred_renderer&   _renderer;
		ecs::Entity_manager& _ecs;

		// TODO: feedback-buffer (dynamic storage-buffer)
		// TODO: active_computation {fence, {weak_ptr<particle-emitter>, particel-buffer, feedback-offset}[] }
	};

	class Particle_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Particle_pass_factory>();
		}

		auto create_pass(Deferred_renderer&, util::maybe<ecs::Entity_manager&>, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
