#pragma once

#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>
#include <mirrage/renderer/pass/deferred_lighting_subpass.hpp>

#include <mirrage/renderer/deferred_renderer.hpp>

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
		void handle_obj(Frame_data&                      fd,
		                Culling_mask                     mask,
		                ecs::Entity_facet                entity,
		                ecs::components::Transform_comp& transform,
		                Model_comp&                      model,
		                const Sub_mesh&                  submesh)
		{
			_gpass.handle_obj(fd, mask, entity, transform, model, submesh);
		}
		void handle_obj(Frame_data&                      fd,
		                Culling_mask                     mask,
		                ecs::Entity_facet                entity,
		                ecs::components::Transform_comp& transform,
		                Model_comp&                      model,
		                Skinning_type                    skinning,
		                std::uint32_t                    pose_offset)
		{
			_gpass.handle_obj(fd, mask, entity, transform, model, skinning, pose_offset);
		}

		void handle_obj(Frame_data& fd, Culling_mask mask, Billboard& bb, glm::vec3 pos = {})
		{
			_gpass.handle_obj(fd, mask, bb, pos);
		}
		void handle_obj(Frame_data&                      fd,
		                Culling_mask                     mask,
		                Decal&                           decal,
		                ecs::components::Transform_comp& transform)
		{
			_gpass.handle_obj(fd, mask, decal, transform);
		}
		void handle_obj(Frame_data&           fd,
		                Culling_mask          mask,
		                Particle_system_comp& sys,
		                Particle_emitter&     emitter)
		{
			_gpass.handle_obj(fd, mask, sys, emitter);
		}

		void handle_obj(Frame_data&                      fd,
		                Culling_mask                     mask,
		                ecs::Entity_facet                entity,
		                ecs::components::Transform_comp& transform,
		                Directional_light_comp&          light)
		{
			_lpass.handle_obj(fd, mask, entity, transform, light);
		}
		void handle_obj(Frame_data&                      fd,
		                Culling_mask                     mask,
		                ecs::Entity_facet                entity,
		                ecs::components::Transform_comp& transform,
		                Point_light_comp&                light)
		{
			_lpass.handle_obj(fd, mask, entity, transform, light);
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
