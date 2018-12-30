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
	  : Particle_system(std::move(cfg))
	{
		_position = position;
		_rotation = rotation;
	}
	Particle_system::Particle_system(asset::Ptr<Particle_system_config> c, ecs::Entity_handle follow)
	  : _cfg(std::move(c)), _follow(follow)
	{
		auto&& cfg = *_cfg;

		_emitters.reserve(cfg.emitter.size());
		for(auto& e : cfg.emitter) {
			_emitters.emplace_back(e);
		}

		_effectors.reserve(cfg.effector.size());
		for(auto& e : cfg.effector) {
			_effectors.emplace_back(e);
		}
	}

	auto Particle_system::emitter(int i) -> Particle_emitter_ref
	{
		MIRRAGE_INVARIANT(i >= 0 && i < int(_emitters.size()),
		                  "particle emitter index out of bounds " << i << " >= " << _emitters.size());
		return {std::shared_ptr<Particle_system::Emitter_list>(shared_from_this(), &_emitters), i};
	}

	void load_component(ecs::Deserializer& state, Particle_system_comp& comp)
	{
		auto aid = comp.particle_system ? comp.particle_system->cfg_aid().str() : std::string("");

		state.read_virtual(sf2::vmember("cfg", aid));

		if(aid != comp.particle_system->cfg_aid().str()) {
			if(aid.empty())
				comp.particle_system = {};
			else {
				comp.particle_system = std::make_shared<Particle_system>(
				        state.assets.load<Particle_system_config>(asset::AID(aid)), comp.owner_handle());
			}
		}
	}
	void save_component(ecs::Serializer& state, const Particle_system_comp& comp)
	{
		auto aid = comp.particle_system ? comp.particle_system->cfg_aid().str() : std::string("");
		state.write_virtual(sf2::vmember("cfg", aid));
	}

} // namespace mirrage::renderer

namespace mirrage::asset {

	Loader<renderer::Particle_script>::Loader(graphic::Device&        device,
	                                          vk::DescriptorSetLayout desc_set_layout)
	  : _device(device)
	  , _layout(device.vk_device()->createPipelineLayoutUnique(
	            vk::PipelineLayoutCreateInfo{{}, 1, &desc_set_layout, 1, nullptr}))
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

		sf2::deserialize_json(in,
		                      [&](auto& msg, uint32_t row, uint32_t column) {
			                      LOG(plog::error) << "Error parsing JSON from " << in.aid().str() << " at "
			                                       << row << ":" << column << ": " << msg;
		                      },
		                      r);

		auto loads = std::vector<async::task<void>>();
		loads.reserve(r.emitter.size() * 4);

		for(auto& e : r.emitter) {
			e.emit_script   = in.manager().load<renderer::Particle_script>(e.emit_script_id);
			e.update_script = in.manager().load<renderer::Particle_script>(e.update_script_id);

			if(!e.model_id.empty()) {
				e.model = in.manager().load<renderer::Model>(e.model_id);
				loads.emplace_back(async::when_all(e.model.internal_task(),
				                                   e.emit_script.internal_task(),
				                                   e.update_script.internal_task())
				                           .then([](auto&&...) { return; }));

			} else {
				e.material = in.manager().load<renderer::Material>(e.material_id);
				loads.emplace_back(async::when_all(e.material.internal_task(),
				                                   e.emit_script.internal_task(),
				                                   e.update_script.internal_task())
				                           .then([](auto&&...) { return; }));
			}
		}

		return async::when_all(loads.begin(), loads.end()).then([r = std::move(r)](auto&&...) mutable {
			for(auto& e : r.emitter) {
				if(e.model) {
					MIRRAGE_INVARIANT(!e.model->rigged(), "Animations are not supported for particle-models");
					MIRRAGE_INVARIANT(e.model->sub_meshes().size() == 1,
					                  "Particle-models must have exacly one sub-mesh");

					e.material = e.model->sub_meshes().at(0).material;
				}
			}

			return std::move(r);
		});
	}

} // namespace mirrage::asset
