#include <mirrage/renderer/particle_system.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/utils/ranges.hpp>

#include <vulkan/vulkan.hpp>

namespace mirrage::renderer {

	void Particle_script::bind(vk::CommandBuffer cb)
	{
		cb.bindPipeline(vk::PipelineBindPoint::eCompute, *_pipeline);
	}

	Particle_system::Particle_system(asset::Ptr<Particle_system_config> cfg,
	                                 glm::vec3                          position,
	                                 glm::quat                          rotation)
	  : _cfg(std::move(cfg))
	  , _emitters(util::build_vector(_cfg->emitters.size(),
	                                 [&](auto idx) { return Particle_emitter(_cfg->emitters[idx]); }))
	  , _effectors(_cfg->effectors)
	  , _position(position)
	  , _rotation(rotation)
	{
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

		auto new_aid   = aid;
		auto effectors = std::vector<Particle_effector_config>();
		state.read_virtual(sf2::vmember("cfg", new_aid), sf2::vmember("effectors", effectors));

		if(new_aid != aid) {
			comp.particle_system =
			        new_aid.empty()
			                ? Particle_system{}
			                : Particle_system{state.assets.load<Particle_system_config>(asset::AID(new_aid))};
		}

		if(!effectors.empty())
			comp.particle_system.effectors() = std::move(effectors);
	}
	void save_component(ecs::Serializer& state, const Particle_system_comp& comp)
	{
		auto aid = comp_cfg_aid(comp);
		state.write_virtual(sf2::vmember("cfg", aid));
	}

} // namespace mirrage::renderer

namespace mirrage::asset {

	Loader<renderer::Particle_script>::Loader(graphic::Device&        device,
	                                          vk::DescriptorSetLayout global_uniforms,
	                                          vk::DescriptorSetLayout storage_buffer,
	                                          vk::DescriptorSetLayout uniform_buffer)
	  : _device(device)
	{
		auto desc_sets = std::array<vk::DescriptorSetLayout, 5>{
		        global_uniforms, storage_buffer, storage_buffer, storage_buffer, uniform_buffer};
		auto push_constants = vk::PushConstantRange{vk::ShaderStageFlagBits::eCompute, 0, 4 * 4 * 4 * 2};

		_layout = device.vk_device()->createPipelineLayoutUnique(
		        vk::PipelineLayoutCreateInfo{{}, desc_sets.size(), desc_sets.data(), 1, &push_constants});
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

		sf2::deserialize_json(in,
		                      [&](auto& msg, uint32_t row, uint32_t column) {
			                      LOG(plog::error) << "Error parsing JSON from " << in.aid().str() << " at "
			                                       << row << ":" << column << ": " << msg;
		                      },
		                      r);

		auto loads = std::vector<async::shared_task<renderer::Particle_script>>();
		loads.reserve(r.emitters.size());

		for(auto& e : r.emitters) {
			e.emit_script = in.manager().load<renderer::Particle_script>(e.emit_script_id);
			loads.emplace_back(e.emit_script.internal_task());
		}

		return async::when_all(loads.begin(), loads.end()).then([r = std::move(r)](auto&&...) mutable {
			return std::move(r);
		});
	}

	auto Loader<renderer::Particle_type_config>::load(istream in)
	        -> async::task<renderer::Particle_type_config>
	{
		auto r = renderer::Particle_type_config();

		sf2::deserialize_json(in,
		                      [&](auto& msg, uint32_t row, uint32_t column) {
			                      LOG(plog::error) << "Error parsing JSON from " << in.aid().str() << " at "
			                                       << row << ":" << column << ": " << msg;
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
