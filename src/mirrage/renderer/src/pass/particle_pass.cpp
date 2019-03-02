#include <mirrage/renderer/pass/particle_pass.hpp>

#include <mirrage/renderer/particle_system.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/entity_manager.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/window.hpp>


namespace mirrage::renderer {

	using namespace graphic;
	using ecs::components::Transform_comp;
	/*
	 * only used in shaders:
		struct Particle {
			glm::vec4     position; // + seed
			glm::vec4     velocity; // + packUnorm2x16(ttl_left, ttl)
		};
	*/

	Particle_pass::Particle_pass(Deferred_renderer& renderer, ecs::Entity_manager& ecs)
	  : _renderer(renderer), _ecs(ecs), _update_fence(renderer.device().create_fence(false))
	{
		ecs.register_component_type<Particle_system_comp>();
	}


	void Particle_pass::update(util::Time dt)
	{
		for(auto& [transform, ps] : _ecs.list<Transform_comp, Particle_system_comp>()) {
			ps.particle_system.position(transform.position);
			ps.particle_system.rotation(transform.orientation);
			for(auto& e : ps.particle_system.emitters())
				e.incr_time(dt.value());
		}
	}

	void Particle_pass::draw(Frame_data&)
	{
		if(!_update_submitted || _update_fence) {
			if(_update_submitted) {
				_update_fence.reset();
				std::swap(_new_particle_buffer, _old_particle_buffer);
				// TODO: update draw-offsets?
			}

			// TODO: find all particle_systems in update-range
			// TODO: per emitter: calc toSpawn and new size
			// TODO: 	calc new particle-count and allocate/resize buffer
			// TODO:	dispatch EmitShader and write to new particle-buffer + feedback-buffer
			// TODO:	Sync then dispatch UpdateShader and write to new particle-buffer + feedback-buffer
		}

		// TODO: find all particle_systems in draw-range
		// TODO:	draw them if they have a particle-buffer assigned
	}


	auto Particle_pass_factory::create_pass(Deferred_renderer&                renderer,
	                                        util::maybe<ecs::Entity_manager&> ecs,
	                                        Engine&,
	                                        bool&) -> std::unique_ptr<Render_pass>
	{
		if(ecs.is_nothing())
			return {};

		return std::make_unique<Particle_pass>(renderer, ecs.get_or_throw());
	}

	auto Particle_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int current_score)
	        -> int
	{
		return current_score;
	}

	void Particle_pass_factory::configure_device(vk::PhysicalDevice,
	                                             util::maybe<std::uint32_t>,
	                                             graphic::Device_create_info&)
	{
	}
} // namespace mirrage::renderer
