#include <mirrage/renderer/particle_system.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/utils/ranges.hpp>

#include <vulkan/vulkan.hpp>

namespace mirrage::renderer {

	void Particle_script::bind(vk::CommandBuffer cb) const
	{
		cb.bindPipeline(vk::PipelineBindPoint::eCompute, *_pipeline);
	}

	void Particle_emitter_gpu_data::set(const std::uint64_t* rev,
	                                    vk::Buffer           buffer,
	                                    vk::DescriptorSet    uniforms,
	                                    std::int32_t         offset,
	                                    std::int32_t         count,
	                                    std::uint32_t        feedback_idx)
	{
		_buffer       = buffer;
		_uniforms     = uniforms;
		_live_rev     = rev;
		_rev          = *rev;
		_offset       = offset;
		_count        = count;
		_feedback_idx = feedback_idx;
	}

	void Particle_emitter::incr_time(float dt)
	{
		_time_accumulator += dt;
		_spawn_entry_timer += dt;
	}

	auto Particle_emitter::spawn(util::default_rand& rand) -> std::int32_t
	{
		if(_cfg->spawn.empty())
			return 0;

		if(_spawn_idx >= _cfg->spawn.size()) {
			_spawn_idx         = 0;
			_spawn_entry_timer = 0;
		}

		auto& entry = _cfg->spawn[_spawn_idx];

		if(_spawn_entry_timer + _time_accumulator > entry.time) {
			_time_accumulator = util::max(1.f / 60, entry.time - _spawn_entry_timer);
			_spawn_idx++;
			_spawn_entry_timer = 0;
		}

		auto pps = (entry.stddev > 0.0f)
		                   ? std::normal_distribution<float>(entry.particles_per_second, entry.stddev)(rand)
		                   : entry.particles_per_second;

		auto spawn = static_cast<std::int32_t>(std::max(0.f, _time_accumulator * pps));

		if(pps > 0.f) {
			_last_timestep = static_cast<float>(spawn) / pps;
			_time_accumulator -= _last_timestep;
		} else {
			_last_timestep    = 1.f / 60.f;
			_time_accumulator = 0;
		}
		_particles_to_spawn = spawn;

		return spawn;
	}
	auto Particle_emitter::gpu_data() -> std::shared_ptr<Particle_emitter_gpu_data>
	{
		if(!_gpu_data)
			_gpu_data = std::make_shared<Particle_emitter_gpu_data>();

		return _gpu_data;
	}

	Particle_system::Particle_system(asset::Ptr<Particle_system_config> cfg,
	                                 glm::vec3                          position,
	                                 glm::quat                          rotation)
	  : _cfg(std::move(cfg))
	  , _loaded(_cfg.ready())
	  , _emitters(!_loaded ? Emitter_list{}
	                       : util::build_vector(
	                               _cfg->emitters.size(),
	                               [&](auto idx) { return Particle_emitter(_cfg->emitters[idx]); }))
	  , _effectors(!_loaded ? Effector_list{} : _cfg->effectors)
	  , _position(position)
	  , _rotation(rotation)
	{
	}
	void Particle_system::_check_reload()
	{
		MIRRAGE_INVARIANT(_cfg.ready(),
		                  "Tried to access Particle_system emitters before the config was laoded!");

		if(!_loaded) {
			_loaded    = true;
			_emitters  = util::build_vector(_cfg->emitters.size(),
                                           [&](auto idx) { return Particle_emitter(_cfg->emitters[idx]); });
			_effectors = _cfg->effectors;
		}
	}

	namespace {
		auto comp_cfg_aid(const Particle_system_comp& comp)
		{
			return comp.particle_system.cfg_aid().process(std::string(), [](auto& aid) { return aid.str(); });
		}
	} // namespace

	void load_component(ecs::Deserializer& state, Particle_system_comp& comp)
	{
		auto aid = comp_cfg_aid(comp);

		auto new_aid = aid;
		state.read_virtual(sf2::vmember("cfg", new_aid));

		if(new_aid != aid) {
			comp.particle_system =
			        new_aid.empty()
			                ? Particle_system{}
			                : Particle_system{state.assets.load<Particle_system_config>(asset::AID(new_aid))};
		}
	}
	void save_component(ecs::Serializer& state, const Particle_system_comp& comp)
	{
		auto aid = comp_cfg_aid(comp);
		state.write_virtual(sf2::vmember("cfg", aid));
	}

	void load_component(ecs::Deserializer& state, Particle_effector_comp& comp) { state.read(comp.effector); }
	void save_component(ecs::Serializer& state, const Particle_effector_comp& comp)
	{
		state.write(comp.effector);
	}


	auto create_particle_shared_desc_set_layout(graphic::Device& device) -> vk::UniqueDescriptorSetLayout
	{
		// global effectors, particles_old, particles_new, feedback_buffer feedback_mapping
		const auto stage = vk::ShaderStageFlagBits::eCompute;

		auto bindings = std::array<vk::DescriptorSetLayoutBinding, 5>{
		        vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eStorageBuffer, 1, stage},
		        vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eStorageBuffer, 1, stage},
		        vk::DescriptorSetLayoutBinding{2, vk::DescriptorType::eStorageBuffer, 1, stage},
		        vk::DescriptorSetLayoutBinding{3, vk::DescriptorType::eStorageBuffer, 1, stage},
		        vk::DescriptorSetLayoutBinding{4, vk::DescriptorType::eStorageBuffer, 1, stage}};

		// vk::DescriptorSetLayoutBinding{5, vk::DescriptorType::eSampledImage, 1, stage},

		return device.create_descriptor_set_layout(bindings);
	}
	auto create_particle_script_pipeline_layout(graphic::Device&        device,
	                                            vk::DescriptorSetLayout shared_desc_set,
	                                            vk::DescriptorSetLayout storage_buffer,
	                                            vk::DescriptorSetLayout) -> vk::UniquePipelineLayout
	{
		// shared_data, emitter/particle_type data
		auto desc_sets      = std::array<vk::DescriptorSetLayout, 2>{shared_desc_set, storage_buffer};
		auto push_constants = vk::PushConstantRange{vk::ShaderStageFlagBits::eCompute, 0, 4 * 4 * 4 * 2};

		return device.vk_device()->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
		        {}, gsl::narrow<std::uint32_t>(desc_sets.size()), desc_sets.data(), 1, &push_constants});
	}

} // namespace mirrage::renderer

namespace mirrage::asset {

	Loader<renderer::Particle_script>::Loader(graphic::Device&        device,
	                                          vk::DescriptorSetLayout storage_buffer,
	                                          vk::DescriptorSetLayout uniform_buffer)
	  : _device(device)
	  , _shared_desc_set(renderer::create_particle_shared_desc_set_layout(device))
	  , _layout(renderer::create_particle_script_pipeline_layout(
	            device, *_shared_desc_set, storage_buffer, uniform_buffer))
	{
	}

	auto Loader<renderer::Particle_script>::load(istream in) -> renderer::Particle_script
	{
		auto code = in.bytes();
		auto module_info =
		        vk::ShaderModuleCreateInfo{{}, code.size(), reinterpret_cast<const uint32_t*>(code.data())};

		auto module = _device.vk_device()->createShaderModuleUnique(module_info);

		auto stage = vk::PipelineShaderStageCreateInfo{
		        {}, vk::ShaderStageFlagBits::eCompute, *module, "main", nullptr};

		return renderer::Particle_script{_device.vk_device()->createComputePipelineUnique(
		        _device.pipeline_cache(), vk::ComputePipelineCreateInfo{{}, stage, *_layout})};
	}


	auto Loader<renderer::Particle_system_config>::load(istream in)
	        -> async::task<renderer::Particle_system_config>
	{
		auto r = renderer::Particle_system_config();

		sf2::deserialize_json(
		        in,
		        [&](auto& msg, uint32_t row, uint32_t column) {
			        LOG(plog::error) << "Error parsing JSON from " << in.aid().str() << " at " << row << ":"
			                         << column << ": " << msg;
		        },
		        r);

		auto loads = std::vector<async::task<void>>();
		loads.reserve(r.emitters.size() * 2u);

		for(auto& e : r.emitters) {
			e.emit_script = in.manager().load<renderer::Particle_script>(e.emit_script_id);
			e.type        = in.manager().load<renderer::Particle_type_config>(e.type_id);

			loads.emplace_back(
			        async::when_all(e.emit_script.internal_task(), e.type.internal_task()).then([] {}));
		}

		return async::when_all(loads.begin(), loads.end()).then([r = std::move(r)]() mutable {
			return std::move(r);
		});
	}

	auto Loader<renderer::Particle_type_config>::load(istream in)
	        -> async::task<renderer::Particle_type_config>
	{
		auto r = renderer::Particle_type_config();

		sf2::deserialize_json(
		        in,
		        [&](auto& msg, uint32_t row, uint32_t column) {
			        LOG(plog::error) << "Error parsing JSON from " << in.aid().str() << " at " << row << ":"
			                         << column << ": " << msg;
		        },
		        r);

		auto script     = in.manager().load<renderer::Particle_script>(r.update_script_id);
		r.update_script = script;

		if(!r.model_id.empty()) {
			auto model = in.manager().load<renderer::Model>(r.model_id);
			r.model    = model;

			return async::when_all(script.internal_task(), model.internal_task())
			        .then([r = std::move(r)](auto&&) mutable {
				        MIRRAGE_INVARIANT(!r.model->rigged(),
				                          "Animations are not supported for particle-models");
				        MIRRAGE_INVARIANT(r.model->sub_meshes().size() == 1,
				                          "Particle-models must have exacly one sub-mesh");
				        r.material = r.model->sub_meshes().at(0).material;
				        return std::move(r);
			        });

		} else {
			auto material = in.manager().load<renderer::Material>(r.material_id);
			r.material    = material;

			return async::when_all(script.internal_task(), material.internal_task())
			        .then([r = std::move(r)](auto&&) mutable { return std::move(r); });
		}
	}

} // namespace mirrage::asset
