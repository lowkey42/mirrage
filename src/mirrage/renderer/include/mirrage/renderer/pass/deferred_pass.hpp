#pragma once

#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>
#include <mirrage/renderer/pass/deferred_lighting_subpass.hpp>

#include <mirrage/renderer/deferred_renderer.hpp>
#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/graphic/render_pass.hpp>


namespace mirrage::renderer {

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

		// object handler for drawing that redirect request to geometry/light subpass
		void handle_obj(Frame_data&              fd,
		                Culling_mask             mask,
		                ecs::Entity_facet        entity,
		                const glm::vec4&         emissive_color,
		                const glm::mat4&         transform,
		                const Model&             model,
		                const Material_override& mat,
		                const Sub_mesh&          submesh)
		{
			_gpass.handle_obj(fd, mask, entity, emissive_color, transform, model, mat, submesh);
		}
		void handle_obj(Frame_data&                        fd,
		                Culling_mask                       mask,
		                ecs::Entity_facet                  entity,
		                const glm::vec4&                   emissive_color,
		                const glm::mat4&                   transform,
		                const Model&                       model,
		                gsl::span<const Material_override> mat,
		                Skinning_type                      skinning,
		                std::uint32_t                      pose_offset)
		{
			_gpass.handle_obj(fd, mask, entity, emissive_color, transform, model, mat, skinning, pose_offset);
		}

		void handle_obj(Frame_data& fd, Culling_mask mask, const Billboard& bb, const glm::vec3& pos = {})
		{
			_gpass.handle_obj(fd, mask, bb, pos);
		}
		void handle_obj(Frame_data& fd, Culling_mask mask, const Decal& decal, const glm::mat4& transform)
		{
			_gpass.handle_obj(fd, mask, decal, transform);
		}
		void handle_obj(Frame_data&             fd,
		                Culling_mask            mask,
		                const glm::vec4&        emissive_color,
		                const Particle_system&  sys,
		                const Particle_emitter& emitter)
		{
			_gpass.handle_obj(fd, mask, emissive_color, sys, emitter);
		}

		void handle_obj(Frame_data&                   fd,
		                Culling_mask                  mask,
		                const glm::quat&              orientation,
		                const glm::vec3&              position,
		                const Directional_light_comp& light)
		{
			_lpass.handle_obj(fd, mask, orientation, position, light);
		}
		void handle_obj(Frame_data&             fd,
		                Culling_mask            mask,
		                const glm::vec3&        position,
		                const Point_light_comp& light)
		{
			_lpass.handle_obj(fd, mask, position, light);
		}


		void update(util::Time dt) override;
		void post_draw(Frame_data&);

		auto name() const noexcept -> const char* override { return "Deferred"; }


	  private:
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
