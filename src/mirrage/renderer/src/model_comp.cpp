#define BUILD_SERIALIZER

#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/ecs/ecs.hpp>
#include <mirrage/ecs/serializer.hpp>

#include <glm/gtx/matrix_decompose.hpp>
#include <sf2/sf2.hpp>


namespace mirrage::renderer {

	void load_component(ecs::Deserializer& state, Material_override_comp& comp)
	{
		auto data = std::unordered_map<int, std::string>();
		state.read_value(data);

		auto max_index = 0;
		for(auto&& [key, value] : data)
			if(key > max_index)
				max_index = key;

		comp.material_overrides.clear();
		comp.material_overrides.resize(max_index);
		for(auto&& [key, value] : data) {
			comp.material_overrides[key].material =
			        state.assets.load<Material>(asset::AID("mat"_strid, value));
		}
	}
	void save_component(ecs::Serializer&, const Material_override_comp&) { MIRRAGE_FAIL("NOT IMPLEMENTED"); }

	void load_component(ecs::Deserializer&, Model_comp&) { MIRRAGE_FAIL("Shouldn't be called directly"); }

	void save_component(ecs::Serializer& state, const Model_comp& comp)
	{
		state.write_virtual(sf2::vmember("aid", comp.model_aid().str()));
	}


	void load_component(ecs::Deserializer& state, Model_unloaded_comp& comp)
	{
		auto aid_str = std::string{};

		auto offset      = glm::vec3();
		auto scale       = glm::vec3();
		auto orientation = glm::quat();
		auto skew        = glm::vec3();
		auto perspective = glm::vec4();
		glm::decompose(comp._local_transform, scale, orientation, offset, skew, perspective);

		state.read_virtual(sf2::vmember("aid", aid_str),
		                   sf2::vmember("offset", offset),
		                   sf2::vmember("scale", scale),
		                   sf2::vmember("orientation", orientation));

		if(!aid_str.empty()) {
			auto aid = asset::AID(aid_str);

			if(aid != comp._model_aid) {
				comp._model_aid = aid;
			}
		}

		comp._local_transform    = glm::toMat4(orientation) * glm::scale(glm::mat4(1.f), scale);
		comp._local_transform[3] = glm::vec4(offset, 1.f);
	}

	void save_component(ecs::Serializer& state, const Model_unloaded_comp& comp)
	{
		state.write_virtual(sf2::vmember("aid", comp._model_aid.str()));
	}


	void load_component(ecs::Deserializer&, Model_loading_comp&)
	{
		MIRRAGE_FAIL("Shouldn't be called directly");
	}

	void save_component(ecs::Serializer& state, const Model_loading_comp& comp)
	{
		state.write_virtual(sf2::vmember("aid", comp.model_aid().str()));
	}
} // namespace mirrage::renderer
