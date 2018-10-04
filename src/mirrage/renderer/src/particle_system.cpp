#include <mirrage/renderer/particle_system.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>

#include <mirrage/utils/ranges.hpp>


namespace mirrage::renderer {

	namespace {
		glm::vec3 spherical_rand(std::mt19937& gen)
		{
			float theta = std::uniform_real_distribution(0.f, 6.283185307179586476925286766559f)(gen);
			float phi   = std::acos(std::uniform_real_distribution(-1.0f, 1.0f)(gen));

			float x = std::sin(phi) * std::cos(theta);
			float y = std::sin(phi) * std::sin(theta);
			float z = std::cos(phi);

			return {x, y, z};
		}

		template <typename T, typename I>
		void erase_by(std::vector<T>& v, const std::vector<I> indices)
		{
			for(auto i : indices) {
				v.erase(v.begin() + i);
			}
		}
	} // namespace

	auto Particle_emitter_config::calc_offset(std::mt19937& gen) -> std::tuple<glm::vec3, glm::vec3>
	{
		switch(shape) {
			case Particle_emitter_shape::sphere: {
				auto dir    = spherical_rand(gen);
				auto radius = std::uniform_real_distribution(0.f, shape_size.x)(gen);
				return {dir *= radius, dir};
			}
		}
		MIRRAGE_FAIL("Unhandled particle shape: " << int(shape));
	}


	Particle_emitter::Particle_emitter(graphic::Texture_ptr                 texture,
	                                   std::vector<Particle_emitter_config> keyframes,
	                                   std::size_t                          capacity,
	                                   util::maybe<ecs::Entity_facet>       follow_entity)
	  : _texture(std::move(texture)), _config_keyframes(std::move(keyframes)), _follow_entity(follow_entity)
	{
		_positions.reserve(capacity);
		_seeds.reserve(capacity);
		_creation_times.reserve(capacity);
		_ttls.reserve(capacity);
		_velocities.reserve(capacity);
	}

	void Particle_emitter::update(util::Time dt, bool emit_new, std::mt19937& gen)
	{
		const auto now = _time.value();

		_follow_entity.process([&](auto& entity) {
			auto transform = entity ? entity.template get<ecs::components::Transform_comp>() : util::nothing;
			if(transform.is_some()) {
				_center_position = transform.get_or_throw().position;
			} else {
				_follow_entity = util::nothing;
			}
		});

		auto config      = _config_keyframes.at(0); // TODO: interpolate
		auto drag_factor = 1.f - config.drag * dt.value();

		// update existing particles
		for(auto i : util::range(_positions.size())) {
			auto vel = _velocities[i];
			_positions[i] += vel * dt.value();
			_velocities[i] *= drag_factor;
		}


		// find dead particles (high indices to low)
		auto dead_count = std::count_if(_ttls.begin(), _ttls.end(), [=](auto t) { return t <= now; });
		_dead_indices.reserve(gsl::narrow<std::size_t>(dead_count));
		auto i = std::int_fast32_t(0);
		for(auto ttl : util::range_reverse(_ttls)) {
			if(ttl <= now)
				_dead_indices.emplace_back(i);

			i++;
		}

		// spawn new particles
		if(_active && emit_new) {
			auto spawn_rate =
			        std::normal_distribution(config.spawn_rate_mean, config.spawn_rate_variance)(gen);
			_to_spawn += spawn_rate * dt.value();
			auto spawn_now = std::size_t(_to_spawn);
			_to_spawn -= spawn_now;

			auto spawn = [&](auto i) {
				auto [offset, direction] = config.calc_offset(gen);
				_positions[i]            = _center_position + offset;
				_seeds[i]                = std::uniform_int_distribution<std::uint32_t>(0)(gen);
				_creation_times[i]       = now;
				_ttls[i] = now + std::normal_distribution(config.ttl_mean, config.ttl_variance)(gen);
				_velocities[i] =
				        direction
				        * std::normal_distribution(config.velocity_mean, config.velocity_variance)(gen);
			};

			// reuse old slots
			while(spawn_now > 0 && !_dead_indices.empty()) {
				spawn(std::size_t(_dead_indices.back()));
				_dead_indices.pop_back();
				spawn_now--;
			}

			// create new slots
			auto new_size = _positions.size() + spawn_now;
			_positions.resize(new_size);
			_seeds.resize(new_size);
			_creation_times.resize(new_size);
			_ttls.resize(new_size);
			_velocities.resize(new_size);

			for(auto i : util::range(spawn_now)) {
				spawn(_positions.size() - 1u - i);
			}
		}

		// remove dead particles
		if(!_dead_indices.empty()) {
			erase_by(_positions, _dead_indices);
			erase_by(_seeds, _dead_indices);
			erase_by(_creation_times, _dead_indices);
			erase_by(_ttls, _dead_indices);
			erase_by(_velocities, _dead_indices);

			_dead_indices.clear();
		}

		_time += dt;
	}


	Particle_system::Particle_system(asset::Asset_manager& assets)
	  : _assets(assets), _random_gen(std::random_device()())
	{
	}

	void Particle_system::add_emitter(std::shared_ptr<Particle_emitter> e)
	{
		_emmitter.emplace_back(std::move(e));
	}
	/*
	namespace {
		struct Emitter_descriptor {
			std::string                          texture_aid;
			std::vector<Particle_emitter_config> keyframes;
		};
		sf2_structDef(Emitter_descriptor, texture_aid, keyframes);
	} // namespace
	*/
	auto Particle_system::add_emitter(asset::AID descriptor) -> std::shared_ptr<Particle_emitter>
	{
		// TODO: load destriptor
		return {};
	}

	void Particle_system::update(util::Time dt)
	{
		for(auto& e : _emmitter) {
			e->update(dt, e.use_count() > 1, _random_gen);
		}

		util::erase_if(_emmitter, [&](auto& e) { return e.use_count() <= 1 && e->dead(); });
	}

} // namespace mirrage::renderer
