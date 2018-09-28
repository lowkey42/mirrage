#include <mirrage/renderer/particle_system.hpp>


namespace mirrage::renderer {
	Particle_emitter::Particle_emitter(graphic::Texture_ptr                 texture,
	                                   std::vector<Particle_emitter_config> keyframes,
	                                   std::size_t                          capacity)
	  : _texture(std::move(texture)), _config_keyframes(std::move(keyframes))
	{
		_positions.reserve(capacity);
		_colors.reserve(capacity);
		_ttl.reserve(capacity);
		_scales.reserve(capacity);
		_rotations.reserve(capacity);
		_velocities.reserve(capacity);
	}

	void Particle_emitter::update(util::Time dt, bool emit_new)
	{
		// TODO: update existing particles
		// TODO: remove dead particles

		if(_active && emit_new) {
			// TODO: spawn new particles
		}
	}


	void Particle_system::add_emitter(std::shared_ptr<Particle_emitter> e)
	{
		_emmitter.emplace_back(std::move(e));
	}
	auto Particle_system::add_emitter(asset::AID descriptor) -> std::shared_ptr<Particle_emitter>
	{
		// TODO: load destriptor
		return {};
	}

	void Particle_system::update(util::Time dt)
	{
		for(auto& e : _emmitter) {
			e->update(dt, e.use_count() > 1);
		}

		util::erase_if(_emmitter, [&](auto& e) { return e.use_count() <= 1 && e->dead(); });
	}

} // namespace mirrage::renderer
