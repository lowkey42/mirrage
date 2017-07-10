#pragma once

#include "model.hpp"

#include <mirrage/ecs/component.hpp>


namespace lux {
namespace renderer {

	class Model_comp : public ecs::Component<Model_comp> {
		public:
			static constexpr const char* name() {return "Model";}
			friend void load_component(ecs::Deserializer& state, Model_comp&);
			friend void save_component(ecs::Serializer& state, const Model_comp&);

			Model_comp() = default;
			Model_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner) {}

			auto model_aid()const -> auto& {return _model_aid;}
			auto model()const -> auto& {return _model;}
			void model(Model_ptr model) {
				_model = std::move(model);
			}

		private:
			asset::AID _model_aid;
			Model_ptr  _model;
	};

}
}
