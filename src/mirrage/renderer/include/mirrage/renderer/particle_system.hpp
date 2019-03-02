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

	struct Particle_color {
		float hue        = 0.f;
		float saturation = 0.f;
		float value      = 0.f;
		float alpha      = 0.f;
	};
	sf2_structDef(Particle_color, hue, saturation, value, alpha);

	template <typename T>
	struct Random_value {
		T mean{};
		T stddev{};
	};
	sf2_structDef(Random_value<float>, mean, stddev);
	sf2_structDef(Random_value<glm::vec4>, mean, stddev);
	sf2_structDef(Random_value<Particle_color>, mean, stddev);


	/// modify velocities of living particles
	/// e.g.
	/// gravity: {..., force=10, dir={0,-1,0}, decay=0, fixed_dir=true}
	/// point: {position=? dir={0,0,0}, decay=2}
	/// flow: {position=? dir={1,0,0}, decay=2}
	struct Particle_effector_config {
		glm::vec3 position{0, 0, 0};
		glm::quat rotation{1, 0, 0, 0};

		float     force = 0.f;
		glm::vec3 force_dir{0, 0, 0};
		float     distance_decay = 2.f;
		bool      fixed_dir      = false;

		bool scale_with_mass = true;
	};
	sf2_structDef(Particle_effector_config,
	              position,
	              rotation,
	              force,
	              force_dir,
	              distance_decay,
	              fixed_dir,
	              scale_with_mass);

	/// describes how living particles are updated and drawn
	struct Particle_type_config {
		Random_value<Particle_color> color        = {{1, 1, 1, 1}};
		Random_value<Particle_color> color_change = {{0, 0, 0, 0}};

		Random_value<glm::vec4> size        = {{1.f, 1.f, 1.f, 0.f}};
		Random_value<glm::vec4> size_change = {{0.f, 0.f, 0.f, 0.f}};

		float base_mass = 1.f;
		float density   = 0.f;

		Random_value<float> sprite_rotation        = {0.0f};
		Random_value<float> sprite_rotation_change = {0.0f};

		Particle_blend_mode blend    = Particle_blend_mode::transparent;
		Particle_geometry   geometry = Particle_geometry::billboard;

		float update_range = -1.f;
		float draw_range   = -1.f;
		bool  shadowcaster = false;

		std::string            material_id;
		renderer::Material_ptr material;

		std::string                 model_id;
		asset::Ptr<renderer::Model> model;

		float drag = 0.f;

		std::string                 update_script_id;
		asset::Ptr<Particle_script> update_script;
	};
	sf2_structDef(Particle_type_config,
	              color,
	              color_change,
	              size,
	              size_change,
	              base_mass,
	              density,
	              sprite_rotation,
	              sprite_rotation_change,
	              blend,
	              geometry,
	              update_range,
	              draw_range,
	              shadowcaster,
	              material_id,
	              model_id,
	              drag,
	              update_script_id);


	struct Particle_emitter_spawn {
		float particles_per_second = 10.f;
		float variance             = 0.f;
		float time                 = -1.f;
	};
	sf2_structDef(Particle_emitter_spawn, particles_per_second, variance, time);

	// describes how new particles are created
	struct Particle_emitter_config {
		util::small_vector<Particle_emitter_spawn, 4> spawn;
		bool                                          spawn_loop = true;

		Random_value<float> ttl = {1.f};

		Random_value<float> velocity = {1.f};

		float parent_velocity = 0.f;

		glm::vec3 offset{0, 0, 0};
		glm::quat rotation{1, 0, 0, 0};

		std::string                 emit_script_id;
		asset::Ptr<Particle_script> emit_script;

		std::string                      type_id;
		asset::Ptr<Particle_type_config> type;
	};
	sf2_structDef(Particle_emitter_config,
	              spawn,
	              spawn_loop,
	              ttl,
	              velocity,
	              parent_velocity,
	              offset,
	              rotation,
	              emit_script_id,
	              type_id);

	struct Particle_system_config {
		util::small_vector<Particle_emitter_config, 1> emitters;
		std::vector<Particle_effector_config>          effectors;
	};
	sf2_structDef(Particle_system_config, emitters, effectors);


	class Particle_emitter {
	  public:
		explicit Particle_emitter(const Particle_emitter_config& cfg) : _cfg(&cfg) {}

		void position(glm::vec3 p) noexcept { _position = p; }
		auto position() const noexcept { return _position; }

		void rotation(glm::quat r) noexcept { _rotation = r; }
		auto rotation() const noexcept { return _rotation; }

		void incr_time(float dt) { _time_accumulator += dt; }

		auto cfg() const noexcept -> auto& { return *_cfg; }

	  private:
		const Particle_emitter_config* _cfg;

		// TODO: userdata?
		glm::vec3 _position{0, 0, 0};
		glm::quat _rotation{1, 0, 0, 0};

		float _time_accumulator = 0.f;
	};

	class Particle_system : private std::enable_shared_from_this<Particle_system> {
	  public:
		using Emitter_list = util::small_vector<Particle_emitter, 1>;

		Particle_system() = default;
		Particle_system(asset::Ptr<Particle_system_config> cfg,
		                glm::vec3                          position = {0, 0, 0},
		                glm::quat                          rotation = {1, 0, 0, 0});

		auto cfg_aid() const { return _cfg ? util::just(_cfg.aid()) : util::nothing; }

		auto emitters() noexcept -> auto& { return _emitters; }
		auto emitters() const noexcept -> auto& { return _emitters; }

		auto effectors() noexcept -> auto& { return _effectors; }
		auto effectors() const noexcept -> auto& { return _effectors; }

		void position(glm::vec3 p) noexcept { _position = p; }
		auto position() const noexcept { return _position; }

		void rotation(glm::quat r) noexcept { _rotation = r; }
		auto rotation() const noexcept { return _rotation; }

	  private:
		asset::Ptr<Particle_system_config>    _cfg;
		Emitter_list                          _emitters;
		std::vector<Particle_effector_config> _effectors;

		glm::vec3 _position{0, 0, 0};
		glm::quat _rotation{1, 0, 0, 0};
	};


	class Particle_system_comp : public ecs::Component<Particle_system_comp> {
	  public:
		static constexpr const char* name() { return "Particle_system"; }
		friend void                  load_component(ecs::Deserializer& state, Particle_system_comp&);
		friend void                  save_component(ecs::Serializer& state, const Particle_system_comp&);

		Particle_system_comp() = default;
		Particle_system_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		Particle_system particle_system;
	};

	class Particle_effector_comp : public ecs::Component<Particle_system_comp> {
	  public:
		static constexpr const char* name() { return "Particle_effector"; }
		friend void                  load_component(ecs::Deserializer& state, Particle_effector_comp&);
		friend void                  save_component(ecs::Serializer& state, const Particle_effector_comp&);

		Particle_effector_comp() = default;
		Particle_effector_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		Particle_effector_config effector;
	};

} // namespace mirrage::renderer

namespace mirrage::asset {

	template <>
	struct Loader<renderer::Particle_script> {
	  public:
		Loader(graphic::Device&        device,
		       vk::DescriptorSetLayout global_uniforms,
		       vk::DescriptorSetLayout storage_buffer,
		       vk::DescriptorSetLayout uniform_buffer);

		auto              load(istream in) -> renderer::Particle_script;
		[[noreturn]] void save(ostream, const renderer::Particle_script&)
		{
			MIRRAGE_FAIL("Save of Particle_script is not supported!");
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
			MIRRAGE_FAIL("Save of Particle_system_config is not supported!");
		}
	};

	template <>
	struct Loader<renderer::Particle_type_config> {
	  public:
		static auto              load(istream in) -> async::task<renderer::Particle_type_config>;
		[[noreturn]] static void save(ostream, const renderer::Particle_type_config&)
		{
			MIRRAGE_FAIL("Save of Particle_type_config is not supported!");
		}
	};

} // namespace mirrage::asset
