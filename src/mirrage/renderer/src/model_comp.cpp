#define BUILD_SERIALIZER

#include <mirrage/renderer/model_comp.hpp>

#include <mirrage/renderer/deferred_renderer.hpp>

#include <mirrage/ecs/serializer.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <sf2/sf2.hpp>


namespace lux {
namespace renderer {

	void load_component(ecs::Deserializer& state, Model_comp& comp) {
		FAIL("Shouldn't be called directly");
	}

	void save_component(ecs::Serializer& state, const Model_comp& comp) {
		state.write_virtual(
			sf2::vmember("aid", comp._model_aid.str())
		);
	}


	void load_component(ecs::Deserializer& state, Model_unloaded_comp& comp) {
		auto aid_str = std::string{};

		state.read_virtual(
			sf2::vmember("aid", aid_str)
		);

		if(!aid_str.empty()) {
			auto aid = asset::AID(aid_str);

			if(aid != comp._model_aid) {
				comp._model_aid = aid;
			}
		}
	}

	void save_component(ecs::Serializer& state, const Model_unloaded_comp& comp) {
		state.write_virtual(
			sf2::vmember("aid", comp._model_aid.str())
		);
	}


	void load_component(ecs::Deserializer&, Model_loading_comp&) {
		FAIL("Shouldn't be called directly");
	}

	void save_component(ecs::Serializer& state, const Model_loading_comp& comp) {
		state.write_virtual(
			sf2::vmember("aid", comp._model_aid.str())
		);
	}

}
}
