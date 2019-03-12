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
		std::string  normal_aid;   // RG: normal
		std::string  brdf_aid;     // R:roughness, G:metallic
		std::string  emission_aid; // R:emissive intensity
	};

#ifdef sf2_structDef
	sf2_structDef(Material_data, substance_id, albedo_aid, normal_aid, brdf_aid, emission_aid);
#endif


	// binds a descriptorSet with all required textures to binding 1
	class Material {
	  public:
		Material(graphic::Device&,
		         graphic::DescriptorSet,
		         vk::Sampler,
		         graphic::Texture_ptr albedo,
		         graphic::Texture_ptr normal,
		         graphic::Texture_ptr brdf,
		         graphic::Texture_ptr emission,
		         bool                 has_albedo,
		         bool                 has_normal,
		         bool                 has_brdf,
		         bool                 has_emission,
		         util::Str_id         substance_id);

		void bind(graphic::Render_pass& pass) const;

		auto substance_id() const noexcept { return _substance_id; }
		auto has_albedo() const noexcept { return _has_albedo; }
		auto has_normal() const noexcept { return _has_normal; }
		auto has_brdf() const noexcept { return _has_brdf; }
		auto has_emission() const noexcept { return _has_emission; }

	  private:
		graphic::DescriptorSet _descriptor_set;
		graphic::Texture_ptr   _albedo;
		graphic::Texture_ptr   _normal;
		graphic::Texture_ptr   _brdf;
		graphic::Texture_ptr   _emission;
		util::Str_id           _substance_id;
		bool                   _has_albedo;
		bool                   _has_normal;
		bool                   _has_brdf;
		bool                   _has_emission;
	};
	using Material_ptr = asset::Ptr<Material>;

	struct Model_vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 tex_coords;

		Model_vertex() = default;
		Model_vertex(glm::vec3 position, glm::vec3 normal, glm::vec2 tex_coords)
		  : position(position), normal(normal), tex_coords(tex_coords)
		{
		}
		Model_vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v)
		  : position(px, py, pz), normal(nx, ny, nz), tex_coords(u, v)
		{
		}
	};
	static_assert(sizeof(Model_vertex) == 4 * (3 + 3 + 2), "Model_vertex has unexpected alignment!");

	struct Model_rigged_vertex {
		glm::vec3  position;
		glm::vec3  normal;
		glm::vec2  tex_coords;
		glm::ivec4 bone_ids;
		glm::vec4  bone_weights;

		Model_rigged_vertex() = default;
		Model_rigged_vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v)
		  : position(px, py, pz)
		  , normal(nx, ny, nz)
		  , tex_coords(u, v)
		  , bone_ids{0, 0, 0, 0}
		  , bone_weights{0, 0, 0, 0}
		{
		}
	};
	static_assert(sizeof(Model_rigged_vertex) == 4 * (3 + 3 + 2 + 4 + 4),
	              "Model_rigged_vertex has unexpected alignment!");


	/*
	* File format:
	* |   0   |   1   |   2   |  3   |
	* |   M   |   M   |   F   |  F   |
	* |    VERSION    |     FLAGS    |		flags: rigged, reserved...
	* |          VERTEX SIZE         |		size of all vertices in bytes
	* |           INDEX SIZE         |		size of all indices in bytes
	* |         SUB MESH COUNT       |
	* |    BOUNDING SPHERE RADIUS    |
	* |   BOUNDING SPHERE X OFFSET   |
	* |   BOUNDING SPHERE Y OFFSET   |
	* |   BOUNDING SPHERE Z OFFSET   |
	* |          BONE COUNT          |		if rigged
	*
	* |          INDEX OFFSET        |
	* |          INDEX COUNT         |
	* |    BOUNDING SPHERE RADIUS    |
	* |   BOUNDING SPHERE X OFFSET   |
	* |   BOUNDING SPHERE Y OFFSET   |
	* |   BOUNDING SPHERE Z OFFSET   |
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
	* |           BONE ID 1          |		if rigged
	* |           BONE ID 2          |		if rigged
	* |           BONE ID 3          |		if rigged
	* |           BONE ID 4          |		if rigged
	* |         BONE WEIGHT 1        |		if rigged
	* |         BONE WEIGHT 2        |		if rigged
	* |         BONE WEIGHT 3        |		if rigged
	* |         BONE WEIGHT 4        |		if rigged
	* x VERTEX COUNT
	*
	* |          VERTEX INDEX        |
	* x INDEX COUNT
	* |   M   |   M   |   F   |  F   |
	*/
	struct Model_file_header {
		static constexpr std::uint32_t type_tag_value = ('F' << 24) | ('F' << 16) | ('M' << 8) | 'M';
		static constexpr std::uint16_t version_value  = 2;

		// wards against wrong/corrupted files and different endianess
		std::uint32_t type_tag = type_tag_value;
		std::uint16_t version  = version_value;
		std::uint16_t flags;

		std::uint32_t vertex_size;
		std::uint32_t index_size;
		std::uint32_t submesh_count;

		float bounding_sphere_radius;
		float bounding_sphere_offset_x;
		float bounding_sphere_offset_y;
		float bounding_sphere_offset_z;
	};
	static_assert(sizeof(Model_file_header) == 4 * 9, "Model_file_header has unexpected size!");


	struct Sub_mesh {
		Material_ptr  material;
		std::uint32_t index_offset;
		std::uint32_t index_count;
		glm::vec3     bounding_sphere_offset;
		float         bounding_sphere_radius;

		Sub_mesh() = default;
		Sub_mesh(Material_ptr  mat,
		         std::uint32_t index_offset,
		         std::uint32_t index_count,
		         glm::vec3     bounds_offset,
		         float         bounds_radius)
		  : material(std::move(mat))
		  , index_offset(index_offset)
		  , index_count(index_count)
		  , bounding_sphere_offset(bounds_offset)
		  , bounding_sphere_radius(bounds_radius)
		{
		}
	};

	class Model {
	  public:
		Model(graphic::Mesh           mesh,
		      std::vector<Sub_mesh>   sub_meshes,
		      float                   bounding_sphere_radius,
		      glm::vec3               bounding_sphere_offset,
		      bool                    rigged,
		      std::int_fast32_t       bone_count,
		      util::maybe<asset::AID> aid = util::nothing);

		auto aid() const noexcept -> auto& { return _aid; }

		// binds the vertices and indices
		void bind_mesh(const graphic::Command_buffer&, std::uint32_t vertex_binding) const;

		// binds the material of the sub mesh and returns its index range (offset, count)
		auto bind_sub_mesh(graphic::Render_pass&, std::size_t index) const
		        -> std::tuple<std::uint32_t, std::uint32_t, const Material*>;

		// binds all sub meshes and calls a callback after binding each
		template <typename F>
		void bind(const graphic::Command_buffer& cb,
		          graphic::Render_pass&          pass,
		          std::uint32_t                  vertex_binding,
		          F&&                            on_sub_mesh) const
		{
			bind_mesh(cb, vertex_binding);

			for(auto& sm : _sub_meshes) {
				sm.material->bind(pass);
				on_sub_mesh(sm.material, sm.index_offset, sm.index_count);
			}
		}

		auto bounding_sphere_radius() const noexcept { return _bounding_sphere_radius; }
		auto bounding_sphere_offset() const noexcept { return _bounding_sphere_offset; }

		auto sub_meshes() const noexcept -> auto& { return _sub_meshes; }
		auto rigged() const noexcept { return _rigged; }
		auto bone_count() const noexcept { return _bone_count; }

		auto ready() const { return _mesh.ready(); }

	  private:
		graphic::Mesh           _mesh;
		std::vector<Sub_mesh>   _sub_meshes;
		util::maybe<asset::AID> _aid;
		float                   _bounding_sphere_radius;
		glm::vec3               _bounding_sphere_offset;
		bool                    _rigged;
		std::int_fast32_t       _bone_count;
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
		  : _device(device), _assets(assets), _owner_qfamily(owner_qfamily)
		{
		}

		auto load(istream in) -> async::task<renderer::Model>;
		void save(ostream, const renderer::Material&) { MIRRAGE_FAIL("Save of materials is not supported!"); }

	  private:
		graphic::Device&      _device;
		asset::Asset_manager& _assets;
		std::uint32_t         _owner_qfamily;
	};

} // namespace mirrage::asset
