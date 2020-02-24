#pragma once

#include "model.hpp"

#include <mirrage/ecs/component.hpp>
#include <mirrage/utils/small_vector.hpp>


namespace mirrage::renderer {

	// model components are intially loaded as Model_unloaded_comp, replaced by Model_loading_comp
	//   once the loading of the model data has begun and finally replaced by Model_comp afterwards.
	// Client code should only care about the Model_comp class as it represents an actually usable model.


	class Material_property_comp : public ecs::Component<Material_property_comp> {
	  public:
		static constexpr const char* name() { return "Material_property"; }

		Material_property_comp() = default;
		Material_property_comp(ecs::Entity_handle   owner,
		                       ecs::Entity_manager& em,
		                       glm::vec4            emissive_color = glm::vec4(1, 1, 1, 1000.f))
		  : Component(owner, em), emissive_color(emissive_color)
		{
		}

		glm::vec4 emissive_color = glm::vec4(1, 1, 1, 1000.f);
	};
	sf2_structDef(Material_property_comp, emissive_color);

	struct Material_override {
		Material_ptr material;
	};

	class Material_override_comp : public ecs::Component<Material_override_comp> {
	  public:
		static constexpr const char* name() { return "Material_override"; }
		friend void                  load_component(ecs::Deserializer& state, Material_override_comp&);
		friend void                  save_component(ecs::Serializer& state, const Material_override_comp&);

		Material_override_comp() = default;
		Material_override_comp(ecs::Entity_handle                              owner,
		                       ecs::Entity_manager&                            em,
		                       const util::small_vector<Material_override, 4>& material_overrides = {})
		  : Component(owner, em), material_overrides(material_overrides)
		{
		}

		util::small_vector<Material_override, 4> material_overrides;
	};


	class Model_comp : public ecs::Component<Model_comp> {
	  public:
		static constexpr const char* name() { return "Model_loaded"; }
		static constexpr const char* name_save_as() { return "Model"; }
		friend void                  load_component(ecs::Deserializer& state, Model_comp&);
		friend void                  save_component(ecs::Serializer& state, const Model_comp&);

		Model_comp() = default;
		Model_comp(ecs::Entity_handle   owner,
		           ecs::Entity_manager& em,
		           Model_ptr            model           = {},
		           const glm::mat4&     local_transform = glm::mat4(1.f))
		  : Component(owner, em)
		  , _model(std::move(model))
		  , _local_transform(local_transform)
		  , _bounding_sphere_offset(_model->bounding_sphere_offset())
		  , _bounding_sphere_radius(_model->bounding_sphere_radius())
		{
		}

		auto local_transform() const noexcept -> auto& { return _local_transform; }
		auto model_aid() const -> auto& { return _model.aid(); }
		auto model() const -> auto& { return _model; }
		void model(Model_ptr model) { _model = std::move(model); }

		auto bounding_sphere_radius() const noexcept { return _bounding_sphere_radius; }
		auto bounding_sphere_offset() const noexcept { return _bounding_sphere_offset; }

	  private:
		Model_ptr _model;
		glm::mat4 _local_transform = glm::mat4(1.f);
		glm::vec3 _bounding_sphere_offset;
		float     _bounding_sphere_radius;
	};

	class Model_unloaded_comp : public ecs::Component<Model_unloaded_comp> {
	  public:
		static constexpr const char* name() { return "Model"; }
		friend void                  load_component(ecs::Deserializer& state, Model_unloaded_comp&);
		friend void                  save_component(ecs::Serializer& state, const Model_unloaded_comp&);

		Model_unloaded_comp() = default;
		Model_unloaded_comp(ecs::Entity_handle   owner,
		                    ecs::Entity_manager& em,
		                    asset::AID           model_aid       = {},
		                    const glm::mat4&     local_transform = glm::mat4(1.f))
		  : Component(owner, em), _model_aid(std::move(model_aid)), _local_transform(local_transform)
		{
		}

		auto local_transform() const noexcept -> auto& { return _local_transform; }
		auto model_aid() const -> auto& { return _model_aid; }

	  private:
		asset::AID _model_aid;
		glm::mat4  _local_transform = glm::mat4(1.f);
	};

	class Model_loading_comp : public ecs::Component<Model_loading_comp> {
	  public:
		static constexpr const char* name() { return "Model_loading"; }
		static constexpr const char* name_save_as() { return "Model"; }
		friend void                  load_component(ecs::Deserializer& state, Model_loading_comp&);
		friend void                  save_component(ecs::Serializer& state, const Model_loading_comp&);

		Model_loading_comp() = default;
		Model_loading_comp(ecs::Entity_handle   owner,
		                   ecs::Entity_manager& em,
		                   Model_ptr            model           = {},
		                   const glm::mat4&     local_transform = glm::mat4(1.f))
		  : Component(owner, em), _model(std::move(model)), _local_transform(local_transform)
		{
		}

		auto local_transform() const noexcept -> auto& { return _local_transform; }
		auto model_aid() const -> auto& { return _model.aid(); }
		auto model() const -> auto& { return _model; }
		auto model() -> auto& { return _model; }

	  private:
		Model_ptr _model;
		glm::mat4 _local_transform = glm::mat4(1.f);
	};
} // namespace mirrage::renderer
