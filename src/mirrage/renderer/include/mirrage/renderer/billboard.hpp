#pragma once

#include <mirrage/renderer/model.hpp>

#include <mirrage/ecs/component.hpp>
#include <mirrage/utils/sf2_glm.hpp>
#include <mirrage/utils/small_vector.hpp>


namespace mirrage::renderer {

	struct Billboard_data {
		glm::vec3  offset{0, 0, 0};
		glm::vec2  size{1, 1};
		glm::vec4  clip_rect{0, 0, 1, 1}; // x,y,w,h
		util::Rgba color{1, 1, 1, 1};
		util::Rgba emissive_color{1, 1, 1, 1000};

		bool active                = true;
		bool dynamic_lighting      = true;
		bool absolute_screen_space = false;
		bool fixed_screen_size     = false;
		bool vertical_rotation     = false;

		std::string material_aid;
	};
#ifdef sf2_structDef
	sf2_structDef(Billboard_data,
	              offset,
	              size,
	              clip_rect,
	              color,
	              emissive_color,
	              dynamic_lighting,
	              absolute_screen_space,
	              fixed_screen_size,
	              vertical_rotation,
	              material_aid);
#endif

	struct Billboard : Billboard_data {
		Material_ptr material;

		Billboard() = default;
		Billboard(Billboard_data&& data, Material_ptr m)
		  : Billboard_data(std::move(data)), material(std::move(m))
		{
		}
	};

	class Billboard_comp : public ecs::Component<Billboard_comp> {
	  public:
		static constexpr const char* name() { return "Billboard"; }
		friend void                  load_component(ecs::Deserializer& state, Billboard_comp&);
		friend void                  save_component(ecs::Serializer& state, const Billboard_comp&);

		using Component::Component;

		util::small_vector<Billboard, 1> billboards;
	};

	struct Billboard_push_constants {
		glm::vec4 position;
		glm::vec4 size; // xy, z=screen_space, w=fixed_screen_size
		glm::vec4 clip_rect;
		glm::vec4 color;
		glm::vec4 emissive_color;
		glm::vec4 placeholder[3];
	};
	extern auto construct_push_constants(const Billboard&, const glm::mat4& view, const glm::vec4& viewport)
	        -> Billboard_push_constants;

} // namespace mirrage::renderer

namespace mirrage::asset {

	template <>
	struct Loader<renderer::Billboard> {
	  public:
		static auto load(istream in) -> async::task<renderer::Billboard>;
		void        save(ostream, const renderer::Billboard&)
		{
			MIRRAGE_FAIL("Save of billboards is not supported!");
		}
	};

} // namespace mirrage::asset
