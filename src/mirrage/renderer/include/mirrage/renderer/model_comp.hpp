#pragma once

#include "model.hpp"

#include <mirrage/ecs/component.hpp>

#include <future>


namespace mirrage {
namespace renderer {

	// model components are intially loaded as Model_unloaded_comp, replaced by Model_loading_comp
	//   once the loading of the model data has begun and finally replaced by Model_comp afterwards.
	// Client code should only care about the Model_comp class as it represents an actually usable model.


	class Model_comp : public ecs::Component<Model_comp> {
		public:
			static constexpr const char* name() {return "Model_loaded";}
			static constexpr const char* name_save_as() {return "Model";}
			friend void load_component(ecs::Deserializer& state, Model_comp&);
			friend void save_component(ecs::Serializer& state, const Model_comp&);

			Model_comp() = default;
			Model_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner,
			           asset::AID model_aid={}, Model_ptr  model={})
			    : Component(manager, owner)
			    , _model_aid(std::move(model_aid))
			    , _model(std::move(model)) {}

			auto model_aid()const -> auto& {return _model_aid;}
			auto model()const -> auto& {return _model;}
			void model(Model_ptr model) {
				_model = std::move(model);
			}

		private:
			asset::AID _model_aid;
			Model_ptr  _model;
	};

	class Model_unloaded_comp : public ecs::Component<Model_unloaded_comp> {
		public:
			static constexpr const char* name() {return "Model";}
			friend void load_component(ecs::Deserializer& state, Model_unloaded_comp&);
			friend void save_component(ecs::Serializer& state, const Model_unloaded_comp&);

			Model_unloaded_comp() = default;
			Model_unloaded_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner,
			                    asset::AID model_aid={})
			    : Component(manager, owner), _model_aid(std::move(model_aid)) {}

			auto model_aid()const -> auto& {return _model_aid;}

		private:
			asset::AID _model_aid;
	};

	class Model_loading_comp : public ecs::Component<Model_loading_comp> {
		public:
			static constexpr const char* name() {return "Model_loading";}
			static constexpr const char* name_save_as() {return "Model";}
			friend void load_component(ecs::Deserializer& state, Model_loading_comp&);
			friend void save_component(ecs::Serializer& state, const Model_loading_comp&);

			Model_loading_comp() = default;
			Model_loading_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner,
                               asset::AID model_aid={}, future<Model_ptr> model={})
			    : Component(manager, owner), _model_aid(std::move(model_aid))
			    , _model(std::move(model)) {}

			auto model_aid()const -> auto& {return _model_aid;}
			auto model()const -> auto& {return _model;}
            auto model() -> auto& {return _model;}

		private:
			asset::AID _model_aid;
            future<Model_ptr> _model;
	};

}
}
