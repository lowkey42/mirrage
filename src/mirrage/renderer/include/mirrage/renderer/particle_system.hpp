#pragma once

#include <mirrage/renderer/model.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/ecs/entity_manager.hpp>
#include <mirrage/graphic/texture.hpp>
#include <mirrage/utils/random.hpp>
#include <mirrage/utils/sf2_glm.hpp>
#include <mirrage/utils/small_vector.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/vec3.hpp>
#include <sf2/sf2.hpp>

#include <memory>
#include <tuple>


namespace mirrage::renderer {

	struct Particle {
		glm::vec4 position; // xyz + uintBitsToFloat(last_feedback_buffer_index)
		glm::vec4 velocity; // xyz + seed
		glm::vec4 ttl;      // ttl_left, ttl_initial, keyframe, keyframe_interpolation_factor
	};

	class Particle_script {
	  public:
		explicit Particle_script(vk::UniquePipeline pipeline) : _pipeline(std::move(pipeline)) {}

		void bind(vk::CommandBuffer) const;

	  private:
		vk::UniquePipeline _pipeline;
	};

	enum class Particle_blend_mode { solid, transparent };
	sf2_enumDef(Particle_blend_mode, solid, transparent);

	enum class Particle_geometry { billboard, mesh };
	sf2_enumDef(Particle_geometry, billboard, mesh);

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
	sf2_structDef(Random_value<glm::quat>, mean, stddev);
	sf2_structDef(Random_value<Particle_color>, mean, stddev);


	/// modify velocities of living particles
	/// e.g.
	/// gravity: {..., force=10, dir={0,-1,0}, decay=0, fixed_dir=true}
	/// point: {position=? dir={0,0,0}, decay=2}
	/// flow: {position=? dir={1,0,0}, decay=2}
	struct Particle_effector_config {
		glm::quat rotation{1, 0, 0, 0};

		glm::vec3 position{0, 0, 0};
		float     force = 0.f;
		glm::vec3 force_dir{0, 0, 0};
		float     distance_decay = 2.f;

		bool fixed_dir       = false; //< ignore position of effector when calculating the force
		bool scale_with_mass = true;
		bool absolute        = false;
	};
	sf2_structDef(Particle_effector_config,
	              position,
	              rotation,
	              force,
	              force_dir,
	              distance_decay,
	              fixed_dir,
	              scale_with_mass);

	struct Particle_keyframe {
		Random_value<Particle_color> color    = {{1, 1, 1, 1}};
		Random_value<glm::vec4>      rotation = {{0.f, 0.f, 0.f, 0.f}};
		Random_value<glm::vec4>      size     = {{1.f, 1.f, 1.f, 0.f}};

		float time      = 0;
		float base_mass = 1;
		float density   = 0;
		float drag      = 0.f;
	};
	sf2_structDef(Particle_keyframe, color, rotation, size, time, base_mass, density, drag);

	/// describes how living particles are updated and drawn
	struct Particle_type_config {
		util::small_vector<Particle_keyframe, 3> keyframes;

		bool color_normal_distribution_h    = true;
		bool color_normal_distribution_s    = true;
		bool color_normal_distribution_v    = true;
		bool color_normal_distribution_a    = true;
		bool rotation_normal_distribution_x = true;
		bool rotation_normal_distribution_y = true;
		bool rotation_normal_distribution_z = true;
		bool size_normal_distribution_x     = true;
		bool size_normal_distribution_y     = true;
		bool size_normal_distribution_z     = true;

		bool rotate_with_velocity = false;

		Particle_blend_mode blend    = Particle_blend_mode::transparent;
		Particle_geometry   geometry = Particle_geometry::billboard;

		float update_range = -1.f;
		float draw_range   = -1.f;
		bool  shadowcaster = false;

		std::string            material_id;
		renderer::Material_ptr material;

		std::string                 model_id;
		asset::Ptr<renderer::Model> model;

		std::string                 update_script_id;
		asset::Ptr<Particle_script> update_script;
	};
	sf2_structDef(Particle_type_config,
	              keyframes,
	              color_normal_distribution_h,
	              color_normal_distribution_s,
	              color_normal_distribution_v,
	              color_normal_distribution_a,
	              rotation_normal_distribution_x,
	              rotation_normal_distribution_y,
	              rotation_normal_distribution_z,
	              size_normal_distribution_x,
	              size_normal_distribution_y,
	              size_normal_distribution_z,
	              rotate_with_velocity,
	              blend,
	              geometry,
	              update_range,
	              draw_range,
	              shadowcaster,
	              material_id,
	              model_id,
	              update_script_id);


	struct Particle_emitter_spawn {
		float particles_per_second = 10.f;
		float stddev               = 0.f;
		float time                 = -1.f;
	};
	sf2_structDef(Particle_emitter_spawn, particles_per_second, stddev, time);

	// describes how new particles are created
	struct Particle_emitter_config {
		Random_value<float> ttl = {1.f};

		Random_value<float> velocity = {1.f};

		float parent_velocity = 0.f;

		glm::vec3 offset{0, 0, 0};
		glm::quat rotation{1, 0, 0, 0};

		util::small_vector<Particle_emitter_spawn, 4> spawn;
		bool                                          spawn_loop = true;

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


	class Particle_emitter_gpu_data {
	  public:
		auto valid() const noexcept { return _live_rev && *_live_rev == _rev; }
		void set(const std::uint64_t* rev,
		         vk::Buffer,
		         vk::DescriptorSet,
		         std::int32_t  offset,
		         std::int32_t  count,
		         std::uint32_t feedback_idx);

		void next_uniforms(vk::DescriptorSet s) { _next_uniforms = s; }
		auto next_uniforms() const noexcept { return _next_uniforms; }

		auto batch_able() const noexcept { return _batch_able; }
		void batch_able(bool b) noexcept { _batch_able = b; }

	  private:
		vk::Buffer           _buffer;
		vk::DescriptorSet    _uniforms;
		vk::DescriptorSet    _next_uniforms;
		const std::uint64_t* _live_rev     = nullptr;
		std::uint64_t        _rev          = 0;
		std::int32_t         _offset       = 0;
		std::int32_t         _count        = 0;
		std::uint32_t        _feedback_idx = 0;
		bool                 _batch_able   = true;

		friend class Particle_emitter;
	};

	class Particle_emitter {
	  public:
		explicit Particle_emitter(const Particle_emitter_config& cfg) : _cfg(&cfg) {}

		void active(bool b) noexcept { _active = b; }
		auto active() const noexcept { return _active; }

		void position(glm::vec3 p) noexcept { _position = p; }
		auto position() const noexcept { return _position; }

		void rotation(glm::quat r) noexcept { _rotation = r; }
		auto rotation() const noexcept { return _rotation; }

		void absolute(bool b) noexcept { _absolute = b; }
		auto absolute() const noexcept { return _absolute; }

		void incr_time(float dt);
		auto spawn(util::default_rand&) -> std::int32_t;
		void override_spawn(std::int32_t spawn) { _particles_to_spawn = spawn; }

		auto drawable() const noexcept { return _gpu_data && _gpu_data->valid(); }
		auto particle_offset() const noexcept { return drawable() ? _gpu_data->_offset : 0; }
		auto particle_count() const noexcept { return drawable() ? _gpu_data->_count : 0; }
		auto particle_feedback_idx() const noexcept
		{
			return drawable() ? util::just(_gpu_data->_feedback_idx) : util::nothing;
		}
		auto particle_buffer() const noexcept { return drawable() ? _gpu_data->_buffer : vk::Buffer{}; }
		auto particle_uniforms() const noexcept
		{
			return drawable() ? _gpu_data->_uniforms : vk::DescriptorSet{};
		}
		auto particles_to_spawn() const noexcept { return _particles_to_spawn; }
		auto last_timestep() const noexcept { return _last_timestep; }

		auto gpu_data() -> std::shared_ptr<Particle_emitter_gpu_data>;

		auto cfg() const noexcept -> auto& { return *_cfg; }

	  private:
		const Particle_emitter_config* _cfg;

		// TODO: userdata?
		bool      _active = true;
		glm::vec3 _position{0, 0, 0};
		glm::quat _rotation{1, 0, 0, 0};
		bool      _absolute = false;

		float       _time_accumulator  = 0.f;
		std::size_t _spawn_idx         = 0;
		float       _spawn_entry_timer = 0;

		std::int32_t _particles_to_spawn = 0;
		float        _last_timestep      = 0;

		// shared_ptr because its update after the async compute tasks finished
		std::shared_ptr<Particle_emitter_gpu_data> _gpu_data;
	};

	class Particle_system : private std::enable_shared_from_this<Particle_system> {
	  public:
		using Emitter_list  = util::small_vector<Particle_emitter, 1>;
		using Effector_list = std::vector<Particle_effector_config>;

		Particle_system() = default;
		Particle_system(asset::Ptr<Particle_system_config> cfg,
		                glm::vec3                          position = {0, 0, 0},
		                glm::quat                          rotation = {1, 0, 0, 0});

		auto cfg() const noexcept { return _cfg; }
		auto cfg_aid() const { return _cfg ? util::just(_cfg.aid()) : util::nothing; }

		auto emitters() noexcept -> auto&
		{
			_check_reload();
			return _emitters;
		}

		auto effectors() noexcept -> auto&
		{
			_check_reload();
			return _effectors;
		}

		void position(glm::vec3 p) noexcept { _position = p; }
		auto position() const noexcept { return _position; }

		void rotation(glm::quat r) noexcept { _rotation = r; }
		auto rotation() const noexcept { return _rotation; }

		auto emitter_position(const Particle_emitter& e) const noexcept
		{
			return e.absolute() ? e.position() : _position + e.position();
		}

		auto emitter_rotation(const Particle_emitter& e) const noexcept
		{
			return e.absolute() ? e.rotation() : glm::normalize(_rotation * e.rotation());
		}

	  private:
		friend class Particle_pass;

		asset::Ptr<Particle_system_config> _cfg;
		bool                               _loaded = false;
		Emitter_list                       _emitters;
		Effector_list                      _effectors;

		glm::vec3 _position{0, 0, 0};
		glm::vec3 _last_position{0, 0, 0};
		glm::quat _rotation{1, 0, 0, 0};

		void _check_reload();
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

	class Particle_effector_comp : public ecs::Component<Particle_effector_comp> {
	  public:
		static constexpr const char* name() { return "Particle_effector"; }
		friend void                  load_component(ecs::Deserializer& state, Particle_effector_comp&);
		friend void                  save_component(ecs::Serializer& state, const Particle_effector_comp&);

		Particle_effector_comp() = default;
		Particle_effector_comp(ecs::Entity_handle owner, ecs::Entity_manager& em) : Component(owner, em) {}

		Particle_effector_config effector;
	};


	extern auto create_particle_shared_desc_set_layout(graphic::Device&) -> vk::UniqueDescriptorSetLayout;

	extern auto create_particle_script_pipeline_layout(graphic::Device&        device,
	                                                   vk::DescriptorSetLayout shared_desc_set,
	                                                   vk::DescriptorSetLayout storage_buffer,
	                                                   vk::DescriptorSetLayout uniform_buffer)
	        -> vk::UniquePipelineLayout;

} // namespace mirrage::renderer

namespace mirrage::asset {

	template <>
	struct Loader<renderer::Particle_script> {
	  public:
		Loader(graphic::Device&        device,
		       vk::DescriptorSetLayout storage_buffer,
		       vk::DescriptorSetLayout uniform_buffer);

		auto              load(istream in) -> renderer::Particle_script;
		[[noreturn]] void save(ostream, const renderer::Particle_script&)
		{
			MIRRAGE_FAIL("Save of Particle_script is not supported!");
		}

	  private:
		graphic::Device&              _device;
		vk::UniqueDescriptorSetLayout _shared_desc_set;
		vk::UniquePipelineLayout      _layout;
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
