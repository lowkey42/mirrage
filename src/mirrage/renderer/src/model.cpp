#include <sf2/sf2.hpp>

#include <mirrage/renderer/model.hpp>

#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>


using namespace mirrage::graphic;

namespace mirrage {
namespace renderer {

	namespace {
		constexpr auto material_textures = 2;

		auto load_or_placeholder(Texture_cache& tex_cache, const std::string& aid) {
			return tex_cache.load(aid.empty() ? "tex:placeholder"_aid : asset::AID("tex"_strid, aid));
		}
	}

	auto create_material_descriptor_set_layout(Device& device, vk::Sampler sampler) -> vk::UniqueDescriptorSetLayout {
		auto bindings = std::array<vk::DescriptorSetLayoutBinding, material_textures>();
		auto samplers = std::array<vk::Sampler, material_textures>();

		std::fill_n(samplers.begin(), material_textures, sampler);

		std::fill_n(bindings.begin(), material_textures, vk::DescriptorSetLayoutBinding {
		                0, vk::DescriptorType::eCombinedImageSampler,
		                1,
		                vk::ShaderStageFlagBits::eFragment,
		                samplers.data()} );

		for(auto i=std::size_t(0); i<material_textures; i++) {
			bindings[i].binding = i;
		}

		return device.create_descriptor_set_layout(bindings);
	}
	
	Material::Material(Device& device, vk::UniqueDescriptorSet descriptor_set, vk::Sampler sampler,
	                   Texture_cache& tex_cache, const Material_data& data)
	    : _descriptor_set(std::move(descriptor_set))
	    , _albedo     (load_or_placeholder(tex_cache, data.albedo_aid))
	    , _mat_data   (load_or_placeholder(tex_cache, data.mat_data_aid))
	    , _material_id(data.substance_id) {

		auto desc_images = std::array<vk::DescriptorImageInfo, material_textures>();
		desc_images[0] = vk::DescriptorImageInfo{sampler, _albedo->view(),
		                                         vk::ImageLayout::eShaderReadOnlyOptimal};
		desc_images[1] = vk::DescriptorImageInfo{sampler, _mat_data->view(),
		                                         vk::ImageLayout::eShaderReadOnlyOptimal};

		auto desc_writes = std::array<vk::WriteDescriptorSet,1>();
		desc_writes[0] = vk::WriteDescriptorSet{*_descriptor_set, 0, 0,
		                                        material_textures,
		                                        vk::DescriptorType::eCombinedImageSampler,
		                                        desc_images.data(), nullptr};

		device.vk_device()->updateDescriptorSets(desc_writes.size(), desc_writes.data(), 0, nullptr);
	}

	void Material::bind(graphic::Render_pass& pass) {
		pass.bind_descriptor_sets(1, {&*_descriptor_set, 1});
	}
	
	
	Model::Model(graphic::Device& device, std::uint32_t owner_qfamily,
	             std::uint32_t vertex_count, std::uint32_t index_count,
	             std::function<void(char*)> write_vertices,
	             std::function<void(char*)> write_indices,
	             std::vector<Sub_mesh> sub_meshes,
	             util::maybe<const asset::AID> aid)
	    : _mesh(device, owner_qfamily, vertex_count, index_count, write_vertices, write_indices)
	    , _sub_meshes(std::move(sub_meshes)), _aid(aid) {
	}

	void Model::bind_mesh(const graphic::Command_buffer& cb,
	                      std::uint32_t vertex_binding) {
		_mesh.bind(cb, vertex_binding);
	}

	auto Model::bind_sub_mesh(graphic::Render_pass& pass,
	                          std::size_t index) -> std::pair<std::size_t, std::size_t> {
		auto& sm = _sub_meshes.at(index);
		sm.material->bind(pass);
		return std::make_pair(sm.index_offset, sm.index_count);
	}
	
	
	Model_loader::Model_loader(Device& device, std::uint32_t owner_qfamily,
	                           Texture_cache& tex_cache, std::size_t max_unique_materials)
	    : _device(device)
	    , _owner_qfamily(owner_qfamily)
	    , _texture_cache(tex_cache)
	    , _sampler(device.create_sampler(12))
	    , _material_descriptor_set_layout(create_material_descriptor_set_layout(device, *_sampler))
	    , _material_descriptor_set_pool(device.create_descriptor_pool(max_unique_materials,
	                                                                  {{vk::DescriptorType::eCombinedImageSampler,
	                                                                    gsl::narrow<std::uint32_t>(max_unique_materials*material_textures)}})) {
	}
	Model_loader::~Model_loader() = default;


	namespace {
		template<typename T>
		void read(std::istream& in, T& value) {
			static_assert(!std::is_pointer<T>::value, "T is a pointer. That is DEFINITLY not what you wanted!");
			in.read(reinterpret_cast<char*>(&value), sizeof(T));
		}

		template<typename T>
		void read(std::istream& in, std::vector<T>& value) {
			static_assert(!std::is_pointer<T>::value, "T is a pointer. That is DEFINITLY not what you wanted!");

			in.read(reinterpret_cast<char*>(value.data()), value.size()*sizeof(T));
		}
	}

	auto  Model_loader::_parse_obj(const asset::AID& aid) -> Model_ptr {
		auto in_mb = _device.context().asset_manager().load_raw(aid);
		if(in_mb.is_nothing()) {
			FAIL("Requested model \"" << aid.str() << "\" doesn't exist!");
		}
		auto& in = in_mb.get_or_throw();

		auto header = Model_file_header{};
		read(in, header);

		if(header.type_tag != Model_file_header::type_tag_value) {
			FAIL("The loaded file \"" << aid.str() << "\" is not a valid model file!");
		}

		if(header.version != Model_file_header::version_value) {
			FAIL("The loaded model file \"" << aid.str() << "\" is not compatible with this version of the application!");
		}

		// load sub meshes
		auto sub_meshes = std::vector<Sub_mesh>();
		sub_meshes.reserve(header.submesh_count);
		for(auto i : util::range(header.submesh_count)) {
			(void) i;

			sub_meshes.emplace_back();
			auto& sub_mesh = sub_meshes.back();
			read(in, sub_mesh.index_offset);
			read(in, sub_mesh.index_count);

			auto material_id_length = std::uint32_t(0);
			read(in, material_id_length);

			auto material_id = std::string();
			material_id.resize(material_id_length);
			in.read(material_id.data(), material_id_length);

			sub_mesh.material = _load_material(asset::AID("mat"_strid, material_id));
		}

		// load data
		auto model = std::make_shared<Model>(_device, _owner_qfamily,
		                                     header.vertex_count, header.index_count,
		                                     [&](char* dest) {in.read(dest, header.vertex_count);},
		                                     [&](char* dest) {in.read(dest, header.index_count);},
		                                     std::move(sub_meshes), aid);

		auto footer = std::uint32_t(0);
		read(in, footer);

		if(footer!=Model_file_header::type_tag_value) {
			WARN("Invalid footer in model file \"" << aid.str() << "\"! The file is probably corrupted!");
		}

		return model;
	}

    auto Model_loader::load(const asset::AID& aid) -> future<Model_ptr> {
		auto& model = _models[aid];

		if(!model) {
			model = _parse_obj(aid);
		}

        return model;
	}

	auto Model_loader::_load_material(const asset::AID& aid) -> Material_ptr {
		auto& mat = _materials[aid];

		if(!mat) {
			auto material_data = _device.context().asset_manager().load<Material_data>(aid, false);

			auto descriptor_set = _material_descriptor_set_pool.create_descriptor(*_material_descriptor_set_layout);
			mat = std::make_shared<Material>(_device,
			                                 std::move(descriptor_set),
			                                 *_sampler, _texture_cache,
			                                 *material_data);
		}

		return mat;
	}

	void Model_loader::shrink_to_fit() {
		util::erase_if(_models, [](const auto& v){return v.second.use_count()<=1;});
		util::erase_if(_materials, [](const auto& v){return v.second.use_count()<=1;});
	}
	
}
}
