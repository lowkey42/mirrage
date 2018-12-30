#include <mirrage/renderer/pass/particle_pass.hpp>

#include <mirrage/graphic/window.hpp>


namespace mirrage::renderer {

	using namespace graphic;
	/*
	 * only used in shaders:
		struct Particle {
			glm::vec3     position;
			glm::vec3     velocity;
			float         ttl;
			std::uint32_t seed;
		};
	*/

	Particle_pass::Particle_pass(Deferred_renderer& renderer, ecs::Entity_manager& ecs)
	  : _renderer(renderer), _ecs(ecs)
	{
	}


	void Particle_pass::update(util::Time)
	{
		// TODO: update all particle_systems (incr. counters)
	}

	void Particle_pass::draw(Frame_data&)
	{
		// TODO: if last async-compute done
		// TODO:	replace particle-buffer and particle-count with result

		// TODO: find all particle_systems in draw-range
		// TODO:	draw them if they have a particle-buffer assigned

		// TODO: if last async-compute done
		// TODO:	find all particle_systems in update-range
		// TODO:	per emitter: calc toSpawn and new size
		// TODO:		calc new particle-count and allocate/resize buffer
		// TODO:		dispatch EmitShader and write to new particle-buffer + feedback-buffer
		// TODO:		Sync then dispatch UpdateShader and write to new particle-buffer + feedback-buffer
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
