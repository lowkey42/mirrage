#pragma once

#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>
#include <mirrage/renderer/pass/deferred_lighting_subpass.hpp>

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

	struct Deferred_push_constants {
		glm::mat4 model;
		glm::vec4 light_color; //< for light-subpass; A=intensity
		glm::vec4 light_data;  //< for light-subpass; R=src_radius, GBA=direction
		glm::vec4 light_data2; //< for light-subpass; R=shadowmapID
		glm::vec4 shadow_color;
	};
	static_assert(sizeof(Deferred_push_constants) <= 4096, "Too large for push constants!");

	class Deferred_pass_factory;

	// populates linear-depth, albedo/matId, matData, writes the direct lighting results
	// to the target color-Buffer and just the diffuse lighting to the other color-Buffer
	class Deferred_pass : public Render_pass {
	  public:
		using Factory = Deferred_pass_factory;

		Deferred_pass(Deferred_renderer&,
		              ecs::Entity_manager&,
		              graphic::Render_target_2D& color_target,
		              graphic::Render_target_2D& color_target_diff);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Deferred"; }

	  private:
		Deferred_renderer& _renderer;

		Deferred_geometry_subpass _gpass;
		Deferred_lighting_subpass _lpass;

		graphic::Framebuffer _gbuffer_framebuffer;
		graphic::Render_pass _render_pass;

		bool _first_frame = true;
	};

	class Deferred_pass_factory : public Render_pass_factory {
	  public:
		auto id() const noexcept -> Render_pass_id override
		{
			return render_pass_id_of<Deferred_pass_factory>();
		}

		auto create_pass(Deferred_renderer&, util::maybe<ecs::Entity_manager&>, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
