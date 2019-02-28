#pragma once

#include <mirrage/renderer/model.hpp>

#include <mirrage/ecs/component.hpp>
#include <mirrage/utils/sf2_glm.hpp>
#include <mirrage/utils/small_vector.hpp>


namespace mirrage::renderer {

	struct Decal_data {
		glm::vec3  offset{0, 0, 0};
		glm::quat  rotation{0.707f, -0.707f, 0.f, 0.f};
		glm::vec2  size{1, 1};
		glm::vec4  clip_rect{0, 0, 1, 1}; // x,y,w,h
		util::Rgba color{1, 1, 1, 1};
		util::Rgba emissive_color{1, 1, 1, 1000};

		float normal_alpha    = 1.f;
		float roughness_alpha = 1.f;
		float metallic_alpha  = 1.f;

		float thickness = 0.1f;
		bool  active    = true;

		std::string material_aid;
	};
#ifdef sf2_structDef
	sf2_structDef(Decal_data,
	              offset,
	              rotation,
	              size,
	              clip_rect,
	              color,
	              emissive_color,
	              normal_alpha,
	              roughness_alpha,
	              metallic_alpha,
	              thickness,
	              active,
	              material_aid);
#endif

	struct Decal : Decal_data {
		Material_ptr material;

		Decal() = default;
		Decal(Decal_data&& data, Material_ptr m) : Decal_data(std::move(data)), material(std::move(m)) {}
	};

	class Decal_comp : public ecs::Component<Decal_comp> {
	  public:
		static constexpr const char* name() { return "Decal"; }
		friend void                  load_component(ecs::Deserializer& state, Decal_comp&);
		friend void                  save_component(ecs::Serializer& state, const Decal_comp&);

		using Component::Component;

		util::small_vector<Decal, 1> decals;
	};

	struct Decal_push_constants {
		glm::mat4 model_view;     // + packed clip_rect (vec4) and color (vec4)
		glm::mat4 model_view_inv; // + emissive_color
	};
	extern auto construct_push_constants(const Decal&, const glm::mat4& model) -> Decal_push_constants;

} // namespace mirrage::renderer

namespace mirrage::asset {

	template <>
	struct Loader<renderer::Decal> {
	  public:
		static auto load(istream in) -> async::task<renderer::Decal>;
		void save(ostream, const renderer::Decal&) { MIRRAGE_FAIL("Save of decals is not supported!"); }
	};

} // namespace mirrage::asset
