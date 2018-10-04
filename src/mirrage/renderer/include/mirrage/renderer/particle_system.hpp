#pragma once

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/ecs/entity_manager.hpp>
#include <mirrage/graphic/texture.hpp>
#include <mirrage/utils/sf2_glm.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/vec3.hpp>
#include <sf2/sf2.hpp>

#include <random>
#include <tuple>


namespace mirrage::renderer {

	enum class Particle_blend_mode { unlit };
	sf2_enumDef(Particle_blend_mode, unlit);

	enum class Particle_emitter_shape { sphere };
	sf2_enumDef(Particle_emitter_shape, sphere);

	template <typename T = std::uint8_t>
	struct Particle_color {
		T hue;
		T saturation;
		T value;
		T alpha;
	};
	sf2_structDef(Particle_color<float>, hue, saturation, value, alpha);
	sf2_structDef(Particle_color<std::uint8_t>, hue, saturation, value, alpha);

	struct Particle_emitter_config {
		float time = 0;

		float spawn_rate_mean     = 10.f;
		float spawn_rate_variance = 1.f;

		Particle_color<float> color_mean            = {1, 1, 1, 1};
		Particle_color<float> color_variance        = {0, 0, 0, 0};
		Particle_color<float> color_change_mean     = {0, 0, 0, 0};
		Particle_color<float> color_change_variance = {0, 0, 0, 0};

		float size_mean            = 0.1f;
		float size_variance        = 0.f;
		float size_change_mean     = 0.f;
		float size_change_variance = 0.f;

		float rotation_mean            = 0.1f;
		float rotation_variance        = 0.1f;
		float rotation_change_mean     = 0.f;
		float rotation_change_variance = 0.f;

		float ttl_mean     = 1.f;
		float ttl_variance = 0.f;

		float velocity_mean     = 1.f;
		float velocity_variance = 0.f;

		Particle_blend_mode    blend      = Particle_blend_mode::unlit;
		Particle_emitter_shape shape      = Particle_emitter_shape::sphere;
		glm::vec3              shape_size = glm::vec3(1, 1, 1);

		float drag = 0.f;

		// returns offset and direction
		auto calc_offset(std::mt19937&) -> std::tuple<glm::vec3, glm::vec3>;
	};
	sf2_structDef(Particle_emitter_config,
	              time,
	              spawn_rate_mean,
	              spawn_rate_variance,
	              color_mean,
	              color_variance,
	              color_change_mean,
	              color_change_variance,
	              size_mean,
	              size_variance,
	              size_change_mean,
	              size_change_variance,
	              rotation_mean,
	              rotation_variance,
	              rotation_change_mean,
	              rotation_change_variance,
	              ttl_mean,
	              ttl_variance,
	              velocity_mean,
	              velocity_variance,
	              blend,
	              shape,
	              shape_size,
	              drag);


	class Particle_emitter {
	  public:
		Particle_emitter(graphic::Texture_ptr                 texture,
		                 std::vector<Particle_emitter_config> keyframes,
		                 std::size_t                          capacity,
		                 util::maybe<ecs::Entity_facet>       follow_entity = {});

		void position(glm::vec3 p)
		{
			_center_position = p;
			_follow_entity   = {};
		}
		void follow(ecs::Entity_facet f) { _follow_entity = f; }

		void update(util::Time dt, bool emit_new, std::mt19937&);
		auto dead() const -> bool { return _positions.empty(); }

		auto active() const noexcept { return _active; }
		void enable() { _active = true; }
		void disable() { _active = false; }

	  private:
		graphic::Texture_ptr                 _texture;
		std::vector<Particle_emitter_config> _config_keyframes;

		std::vector<glm::vec3>     _positions;
		std::vector<std::uint32_t> _seeds;
		std::vector<float>         _creation_times;
		std::vector<float>         _ttls;
		std::vector<glm::vec3>     _velocities;

		std::vector<std::int_fast32_t> _dead_indices;

		util::Time _time     = util::Time{0.f};
		float      _to_spawn = 0.f;
		bool       _active   = true;

		glm::vec3                      _center_position = {0, 0, 0};
		util::maybe<ecs::Entity_facet> _follow_entity;
	};

	class Particle_system {
	  public:
		Particle_system(asset::Asset_manager& assets);

		void add_emitter(std::shared_ptr<Particle_emitter>);
		auto add_emitter(asset::AID descriptor) -> std::shared_ptr<Particle_emitter>;

		void update(util::Time);

	  private:
		asset::Asset_manager& _assets;
		std::mt19937          _random_gen;

		std::vector<std::shared_ptr<Particle_emitter>> _emmitter;
		// TODO
	};
} // namespace mirrage::renderer
