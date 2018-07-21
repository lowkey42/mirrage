#pragma once

#include <mirrage/renderer/animation_comp.hpp>
#include <mirrage/renderer/deferred_renderer.hpp>


namespace mirrage::renderer {

	class Animation_pass : public Render_pass {
	  public:
		Animation_pass(Deferred_renderer&, ecs::Entity_manager&);

		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Animation"; }

	  private:
		Deferred_renderer&   _renderer;
		ecs::Entity_manager& _ecs;

		void _update_animation(Animation_comp& anim, Pose_comp&);
	};

	class Animation_pass_factory : public Render_pass_factory {
	  public:
		auto create_pass(Deferred_renderer&, ecs::Entity_manager&, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
