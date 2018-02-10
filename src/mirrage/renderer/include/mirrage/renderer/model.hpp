#pragma once

#include <mirrage/graphic/mesh.hpp>
#include <mirrage/graphic/texture.hpp>

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <gsl/gsl>

#include <cstdint>
#include <memory>


namespace mirrage::renderer {


	extern auto create_material_descriptor_set_layout(graphic::Device&, vk::Sampler)
	        -> vk::UniqueDescriptorSetLayout;

	struct Material_data {
		util::Str_id substance_id = "default"_strid;
		std::string  albedo_aid;
		std::string  mat_data_aid;
	};

#ifdef sf2_structDef
	sf2_structDef(Material_data, substance_id, albedo_aid, mat_data_aid);
#endif


	// binds a descriptorSet with all required textures to binding 1
	class Material {
	  public:
		Material(graphic::Device&,
		         graphic::DescriptorSet,
		         vk::Sampler,
		         graphic::Texture_ptr albedo,
		         graphic::Texture_ptr mat_data,
		         util::Str_id         material_id);

		void bind(graphic::Render_pass& pass) const;

		auto material_id() const noexcept { return _material_id; }

	  private:
		graphic::DescriptorSet _descriptor_set;
		graphic::Texture_ptr   _albedo;
		graphic::Texture_ptr   _mat_data;
		util::Str_id           _material_id;
	};
	using Material_ptr = asset::Ptr<Material>;


	struct Model_vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 tex_coords;

		Model_vertex() = default;
		Model_vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v)
		  : position(px, py, pz), normal(nx, ny, nz), tex_coords(u, v) {}
	};
	static_assert(sizeof(Model_vertex) == 4 * (3 + 3 + 2), "Model_vertex has unexpected alignment!");


	/*
	* File format:
	* |   0   |   1   |   2   |  3   |
	* |   M   |   M   |   F   |  F   |
	* |            VERSION           |
	* |          VERTEX COUNT        |
	* |          INDEX COUNT         |
	* |         SUB MESH COUNT       |
	*
	* |          INDEX OFFSET        |
	* |          INDEX COUNT         |
	* |       MATERIAL ID LENGTH     |
	*     MATERIAL ID LENGTH bytes
	* x SUB MESH COUNT
	*
	* |           POSITION X         |
	* |           POSITION Y         |
	* |           POSITION Z         |
	* |            NORMAL X          |
	* |            NORMAL Y          |
	* |            NORMAL Z          |
	* |        TEXTURE COORDS S      |
	* |        TEXTURE COORDS T      |
	* x VERTEX COUNT
	*
	* |          VERTEX INDEX        |
	* x INDEX COUNT
	* |   M   |   M   |   F   |  F   |
	*/
	struct Model_file_header {
		static constexpr std::uint32_t type_tag_value = ('F' << 24) | ('F' << 16) | ('M' << 8) | 'M';
		static constexpr std::uint32_t version_value  = 1;

		// wards against wrong/corrupted files and different endianess
		std::uint32_t type_tag = type_tag_value;
		std::uint32_t version  = version_value;

		std::uint32_t vertex_count;
		std::uint32_t index_count;
		std::uint32_t submesh_count;
	};
	static_assert(sizeof(Model_file_header) == 4 * 5, "Model_file_header has unexpected alignment!");


	struct Sub_mesh {
		Material_ptr  material;
		std::uint32_t index_offset;
		std::uint32_t index_count;

		Sub_mesh() = default;
		Sub_mesh(Material_ptr m, std::uint32_t o, std::uint32_t c)
		  : material(std::move(m)), index_offset(o), index_count(c) {}
	};

	class Model {
	  public:
		Model(graphic::Mesh           mesh,
		      std::vector<Sub_mesh>   sub_meshes,
		      util::maybe<asset::AID> aid = util::nothing);

		auto aid() const noexcept -> auto& { return _aid; }

		// binds the vertices and indices
		void bind_mesh(const graphic::Command_buffer&, std::uint32_t vertex_binding) const;

		// binds the material of the sub mesh and returns its index range (offset, count)
		auto bind_sub_mesh(graphic::Render_pass&, std::size_t index) const
		        -> std::pair<std::size_t, std::size_t>;

		// binds all sub meshes and calls a callback after binding each
		template <typename F>
		void bind(const graphic::Command_buffer& cb,
		          graphic::Render_pass&          pass,
		          std::uint32_t                  vertex_binding,
		          F&&                            on_sub_mesh) const {
			bind_mesh(cb, vertex_binding);

			for(auto& sm : _sub_meshes) {
				sm.material->bind(pass);
				on_sub_mesh(sm.material, sm.index_offset, sm.index_count);
			}
		}


	  private:
		graphic::Mesh           _mesh;
		std::vector<Sub_mesh>   _sub_meshes;
		util::maybe<asset::AID> _aid;
	};
	using Model_ptr = asset::Ptr<Model>;

} // namespace mirrage::renderer


namespace mirrage::asset {

	template <>
	struct Loader<renderer::Material> {
	  public:
		Loader(graphic::Device& device, asset::Asset_manager& assets, vk::Sampler, vk::DescriptorSetLayout);

		auto load(istream in) -> async::task<renderer::Material>;
		void save(ostream, const renderer::Material&) { MIRRAGE_FAIL("Save of materials is not supported!"); }

	  private:
		graphic::Device&         _device;
		asset::Asset_manager&    _assets;
		vk::Sampler              _sampler;
		vk::DescriptorSetLayout  _descriptor_set_layout;
		graphic::Descriptor_pool _descriptor_set_pool;
	};

	template <>
	struct Loader<renderer::Model> {
	  public:
		Loader(graphic::Device& device, asset::Asset_manager& assets, std::uint32_t owner_qfamily)
		  : _device(device), _assets(assets), _owner_qfamily(owner_qfamily) {}

		auto load(istream in) -> async::task<renderer::Model>;
		void save(ostream, const renderer::Material&) { MIRRAGE_FAIL("Save of materials is not supported!"); }

	  private:
		graphic::Device&      _device;
		asset::Asset_manager& _assets;
		std::uint32_t         _owner_qfamily;
	};

} // namespace mirrage::asset
