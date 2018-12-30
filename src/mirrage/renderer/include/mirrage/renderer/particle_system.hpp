#pragma once

#include <mirrage/renderer/model.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/ecs/entity_manager.hpp>
#include <mirrage/graphic/texture.hpp>
#include <mirrage/utils/sf2_glm.hpp>
#include <mirrage/utils/small_vector.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/vec3.hpp>
#include <sf2/sf2.hpp>

#include <random>
#include <tuple>


namespace mirrage::renderer {

	// TODO: only for reference during IMPL, delete before merge
	// emitter: {Particle_emitter_config, offset, relative-rotation, follow_entity, time-acc, toSpawn, particle-data, particle-cout, userdata?}
	// affector: {position, rotation, relative/absolute, follow_entity, force:float, dir, decay:float}
	//			gravity: {..., force=10, dir={0,-1,0}, decay=0}
	//			point: {position=? dir={0,0,0}, decay=2}
	//			flow: {position=? dir={1,0,0}, decay=2}
	// particle_system: {shared_ptr<emitter>[], offset, relative-rotation, follow_entity, affector}


	class Particle_script {
	  public:
		explicit Particle_script(vk::UniquePipeline pipeline) : _pipeline(std::move(pipeline)) {}

		void bind(vk::CommandBuffer);

	  private:
		vk::UniquePipeline _pipeline;
	};

	enum class Particle_blend_mode { solid, volumn, transparent };
	sf2_enumDef(Particle_blend_mode, solid, volumn, transparent);

	enum class Particle_geometry { billboard, ribbon, mesh };
	sf2_enumDef(Particle_geometry, billboard, ribbon, mesh);

	template <typename T = std::uint8_t>
	struct Particle_color {
		T hue;
		T saturation;
		T value;
		T alpha;
	};
	sf2_structDef(Particle_color<float>, hue, saturation, value, alpha);
	sf2_structDef(Particle_color<std::uint8_t>, hue, saturation, value, alpha);

	struct Particle_emitter_spawn {
		float spawn_rate_mean     = 10.f;
		float spawn_rate_variance = 1.f;
		float time                = -1.f;
	};
	sf2_structDef(Particle_emitter_spawn, spawn_rate_mean, spawn_rate_variance, time);

	struct Particle_emitter_config {
		float time = 0;

		util::small_vector<Particle_emitter_spawn, 4> spawn;

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

		Particle_blend_mode blend    = Particle_blend_mode::transparent;
		Particle_geometry   geometry = Particle_geometry::billboard;

		std::string            material_id;
		renderer::Material_ptr material;

		std::string                 model_id;
		asset::Ptr<renderer::Model> model;

		float drag            = 0.f;
		float parent_velocity = 0.f;

		glm::vec3 offset{0, 0, 0};
		glm::quat rotation{1, 0, 0, 0};

		std::string emit_script_id;
		std::string update_script_id;

		asset::Ptr<Particle_script> emit_script;
		asset::Ptr<Particle_script> update_script;
	};
	sf2_structDef(Particle_emitter_config,
	              time,
	              spawn,
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
	              geometry,
	              material_id,
	              model_id,
	              drag,
	              parent_velocity,
	              emit_script_id,
	              update_script_id);

	struct Particle_effector_config {
		glm::vec3 position{0, 0, 0};
		glm::quat rotation{1, 0, 0, 0};

		float     force = 0.f;
		glm::vec3 force_dir{0, 0, 0};
		float     distance_decay = 2.f;
	};
	sf2_structDef(Particle_effector_config, position, rotation, force, force_dir, distance_decay);

	struct Particle_system_config {
		util::small_vector<Particle_emitter_config, 1>  emitter;
		util::small_vector<Particle_effector_config, 1> effector;
	};
	sf2_structDef(Particle_system_config, emitter, effector);


	class Particle_emitter {
	  public:
		explicit Particle_emitter(const Particle_emitter_config& cfg) : _cfg(cfg) {}

		void absolute(bool a) noexcept
		{
			_absolute = a;
			_follow   = {};
		}
		auto absolute() const noexcept { return _absolute; }

		void position(glm::vec3 p) noexcept
		{
			_position = p;
			_follow   = {};
		}
		auto position() const noexcept { return _position; }

		void rotation(glm::quat r) noexcept
		{
			_rotation = r;
			_follow   = {};
		}
		auto rotation() const noexcept { return _rotation; }

		void follow(ecs::Entity_handle e) { _follow = e; }
		auto follow() const { return _follow; }

		void incr_time(float dt) { _time_accumulator += dt; }

		/// returns old buffer (that might still be needed by the last frame)
		auto update_data(int count, graphic::Backed_buffer data) -> graphic::Backed_buffer;

	  private:
		const Particle_emitter_config& _cfg;

		// TODO: userdata?
		glm::vec3          _position{0, 0, 0};
		glm::quat          _rotation{1, 0, 0, 0};
		bool               _absolute = false;
		ecs::Entity_handle _follow;

		float                  _time_accumulator = 0.f;
		int                    _particle_count   = 0;
		graphic::Backed_buffer _particle_data;
	};

	class Particle_effector {
	  public:
		explicit Particle_effector(const Particle_effector_config& cfg) : _cfg(cfg) {}

		auto cfg() const noexcept -> auto& { return _cfg; }

		void absolute(bool a) noexcept
		{
			_absolute = a;
			_follow   = {};
		}
		auto absolute() const noexcept { return _absolute; }

		void position(glm::vec3 p) noexcept
		{
			_position = p;
			_follow   = {};
		}
		auto position() const noexcept { return _position; }

		void rotation(glm::quat r) noexcept
		{
			_rotation = r;
			_follow   = {};
		}
		auto rotation() const noexcept { return _rotation; }

		void follow(ecs::Entity_handle e) { _follow = e; }
		auto follow() const { return _follow; }

	  private:
		const Particle_effector_config& _cfg;

		glm::vec3          _position{0, 0, 0};
		glm::quat          _rotation{1, 0, 0, 0};
		bool               _absolute = false;
		ecs::Entity_handle _follow;
	};

	class Particle_emitter_ref;

	class Particle_system : private std::enable_shared_from_this<Particle_system> {
	  public:
		using Emitter_list = util::small_vector<Particle_emitter, 1>;

		Particle_system(asset::Ptr<Particle_system_config> cfg, glm::vec3 position, glm::quat rotation);
		explicit Particle_system(asset::Ptr<Particle_system_config> cfg, ecs::Entity_handle follow = {});

		auto cfg_aid() const { return _cfg.aid(); }

		auto emitters() noexcept -> auto& { return _emitters; }
		auto emitters() const noexcept -> auto& { return _emitters; }

		auto emitter(int i) -> Particle_emitter_ref;
		auto emitter_count() const noexcept { return _emitters.size(); }

		auto effectors() noexcept -> auto& { return _effectors; }
		auto effectors() const noexcept -> auto& { return _effectors; }

		void position(glm::vec3 p) noexcept
		{
			_position = p;
			_follow   = {};
		}
		auto position() const noexcept { return _position; }

		void rotation(glm::quat r) noexcept
		{
			_rotation = r;
			_follow   = {};
		}
		auto rotation() const noexcept { return _rotation; }

		void follow(ecs::Entity_handle e) { _follow = e; }
		auto follow() const { return _follow; }

	  private:
		asset::Ptr<Particle_system_config> _cfg;
		Emitter_list                       _emitters;
		std::vector<Particle_effector>     _effectors;

		glm::vec3          _position{0, 0, 0};
		glm::quat          _rotation{1, 0, 0, 0};
		ecs::Entity_handle _follow;
	};

	using Particle_system_ptr = std::shared_ptr<Particle_system>;


	class Particle_emitter_ref {
	  public:
		auto operator-> () -> Particle_emitter* { return &**this; }
		auto operator-> () const -> Particle_emitter* { return &**this; }
		auto operator*() -> Particle_emitter& { return (*_system_emitters)[_index]; }
		auto operator*() const -> Particle_emitter& { return (*_system_emitters)[_index]; }

	  private:
		using Emitter_ptr = std::shared_ptr<Particle_system::Emitter_list>;

		friend class Particle_system;

		Particle_emitter_ref(Emitter_ptr emitters, int index)
		  : _system_emitters(std::move(emitters)), _index(gsl::narrow<std::size_t>(index))
		{
		}

		Emitter_ptr _system_emitters;
		std::size_t _index;
	};


	class Particle_system_comp : public ecs::Component<Particle_system_comp> {
	  public:
		static constexpr const char* name() { return "Particle_system"; }
		friend void                  load_component(ecs::Deserializer& state, Particle_system_comp&);
		friend void                  save_component(ecs::Serializer& state, const Particle_system_comp&);

		Particle_system_comp() = default;
		Particle_system_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		Particle_system_ptr particle_system;
	};

} // namespace mirrage::renderer

namespace mirrage::asset {

	template <>
	struct Loader<renderer::Particle_script> {
	  public:
		Loader(graphic::Device& device, vk::DescriptorSetLayout);

		auto              load(istream in) -> renderer::Particle_script;
		[[noreturn]] void save(ostream, const renderer::Particle_script&)
		{
			MIRRAGE_FAIL("Save of materials is not supported!");
		}

	  private:
		graphic::Device&         _device;
		vk::UniquePipelineLayout _layout;
	};

	template <>
	struct Loader<renderer::Particle_system_config> {
	  public:
		static auto              load(istream in) -> async::task<renderer::Particle_system_config>;
		[[noreturn]] static void save(ostream, const renderer::Particle_system_config&)
		{
			MIRRAGE_FAIL("Save of materials is not supported!");
		}
	};

} // namespace mirrage::asset
