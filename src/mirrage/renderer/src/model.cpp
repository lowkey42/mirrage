#include <sf2/sf2.hpp>

#include <mirrage/renderer/model.hpp>

#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/utils/ranges.hpp>


using namespace mirrage::graphic;

namespace mirrage::renderer {

	namespace {
		constexpr auto material_textures = std::uint32_t(4);
	} // namespace

	auto create_material_descriptor_set_layout(Device& device, vk::Sampler sampler)
	        -> vk::UniqueDescriptorSetLayout
	{
		auto bindings = std::array<vk::DescriptorSetLayoutBinding, material_textures + 1>();
		auto samplers = std::array<vk::Sampler, material_textures>();

		std::fill_n(samplers.begin(), material_textures, sampler);

		std::fill_n(bindings.begin(),
		            material_textures,
		            vk::DescriptorSetLayoutBinding{0,
		                                           vk::DescriptorType::eCombinedImageSampler,
		                                           1,
		                                           vk::ShaderStageFlagBits::eFragment,
		                                           samplers.data()});

		for(auto i : util::range(material_textures)) {
			bindings[i].binding = i;
		}

		bindings[material_textures] = vk::DescriptorSetLayoutBinding{
		        material_textures, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAll};

		return device.create_descriptor_set_layout(bindings);
	}

	Material::Material(Device&                device,
	                   graphic::DescriptorSet descriptor_set,
	                   vk::Sampler            sampler,
	                   graphic::Texture_ptr   albedo,
	                   graphic::Texture_ptr   normal,
	                   graphic::Texture_ptr   brdf,
	                   graphic::Texture_ptr   emission,
	                   graphic::Static_buffer uniform_buffer,
	                   bool                   has_albedo,
	                   bool                   has_normal,
	                   bool                   has_brdf,
	                   bool                   has_emission,
	                   util::Str_id           substance_id,
	                   glm::vec4              albedo_color,
	                   glm::vec4              emission_color,
	                   float                  roughness,
	                   float                  metallic,
	                   float                  refraction)

	  : _albedo(std::move(albedo))
	  , _normal(std::move(normal))
	  , _brdf(std::move(brdf))
	  , _emission(std::move(emission))
	  , _uniform_buffer(std::move(uniform_buffer))
	  , _descriptor_set(std::move(descriptor_set))
	  , _substance_id(substance_id ? substance_id : "default"_strid)
	  , _has_albedo(has_albedo)
	  , _has_normal(has_normal)
	  , _has_brdf(has_brdf)
	  , _has_emission(has_emission)
	  , _albedo_color(albedo_color)
	  , _emission_color(emission_color)
	  , _roughness(roughness)
	  , _metallic(metallic)
	  , _refraction(refraction)
	{
		auto desc_images = std::array<vk::DescriptorImageInfo, material_textures>{
		        vk::DescriptorImageInfo{sampler, _albedo->view(), vk::ImageLayout::eShaderReadOnlyOptimal},
		        vk::DescriptorImageInfo{sampler, _normal->view(), vk::ImageLayout::eShaderReadOnlyOptimal},
		        vk::DescriptorImageInfo{sampler, _brdf->view(), vk::ImageLayout::eShaderReadOnlyOptimal},
		        vk::DescriptorImageInfo{sampler, _emission->view(), vk::ImageLayout::eShaderReadOnlyOptimal}};

		auto desc_buffer = vk::DescriptorBufferInfo{_uniform_buffer.buffer(), 0, VK_WHOLE_SIZE};

		auto desc_writes = std::array<vk::WriteDescriptorSet, 2>{
		        vk::WriteDescriptorSet{*_descriptor_set,
		                               0,
		                               0,
		                               material_textures,
		                               vk::DescriptorType::eCombinedImageSampler,
		                               desc_images.data(),
		                               nullptr},
		        vk::WriteDescriptorSet{*_descriptor_set,
		                               material_textures,
		                               0,
		                               1,
		                               vk::DescriptorType::eUniformBuffer,
		                               nullptr,
		                               &desc_buffer}};
		device.vk_device()->updateDescriptorSets(
		        std::uint32_t(desc_writes.size()), desc_writes.data(), 0, nullptr);
	}

	void Material::bind(graphic::Render_pass& pass, int bind_point) const
	{
		pass.bind_descriptor_sets(gsl::narrow<std::uint32_t>(bind_point), {_descriptor_set.get_ptr(), 1});
	}


	Model::Model(graphic::Mesh           mesh,
	             std::vector<Sub_mesh>   sub_meshes,
	             float                   bounding_sphere_radius,
	             glm::vec3               bounding_sphere_offset,
	             bool                    rigged,
	             std::int_fast32_t       bone_count,
	             util::maybe<asset::AID> aid)
	  : _mesh(std::move(mesh))
	  , _sub_meshes(std::move(sub_meshes))
	  , _aid(aid)
	  , _bounding_sphere_radius(bounding_sphere_radius)
	  , _bounding_sphere_offset(bounding_sphere_offset)
	  , _rigged(rigged)
	  , _bone_count(bone_count)
	{
	}

	void Model::bind_mesh(const graphic::Command_buffer& cb, std::uint32_t vertex_binding) const
	{
		_mesh.bind(cb, vertex_binding);
	}

	auto Model::bind_sub_mesh(graphic::Render_pass& pass, std::size_t index) const
	        -> std::tuple<std::uint32_t, std::uint32_t, const Material*>
	{
		auto& sm = _sub_meshes.at(index);
		sm.material->bind(pass);
		return std::make_tuple(sm.index_offset, sm.index_count, &*sm.material);
	}

} // namespace mirrage::renderer


namespace mirrage::asset {

	Loader<renderer::Material>::Loader(graphic::Device&        device,
	                                   asset::Asset_manager&   assets,
	                                   vk::Sampler             sampler,
	                                   vk::DescriptorSetLayout layout,
	                                   std::uint32_t           queue_family)
	  : _device(device)
	  , _assets(assets)
	  , _sampler(sampler)
	  , _descriptor_set_layout(layout)
	  , _descriptor_set_pool(*device.vk_device(),
	                         256,
	                         {vk::DescriptorType::eCombinedImageSampler, vk::DescriptorType::eUniformBuffer})
	  , _queue_family(queue_family)
	{
	}

	auto create_material_uniform_buffer(Device&       device,
	                                    std::uint32_t queue_family,
	                                    glm::vec4     albedo_color,
	                                    glm::vec4     emission_color,
	                                    float         roughness,
	                                    float         metallic,
	                                    float         refraction,
	                                    bool          has_normals) -> graphic::Static_buffer
	{
		auto data     = std::array<char, sizeof(float) * (4 * 2 + 4)>();
		auto data_ptr = data.data();
		memcpy(data_ptr, &albedo_color, sizeof(float) * 4);
		data_ptr += sizeof(float) * 4;
		memcpy(data_ptr, &emission_color, sizeof(float) * 4);
		data_ptr += sizeof(float) * 4;
		memcpy(data_ptr, &roughness, sizeof(float));
		data_ptr += sizeof(float);
		memcpy(data_ptr, &metallic, sizeof(float));
		data_ptr += sizeof(float);
		memcpy(data_ptr, &refraction, sizeof(float));
		data_ptr += sizeof(float);
		float has_normals_f = has_normals ? 1.f : 0.f;
		memcpy(data_ptr, &has_normals_f, sizeof(float));

		return device.transfer().upload_buffer(vk::BufferUsageFlagBits::eUniformBuffer, queue_family, data);
	}

	auto Loader<renderer::Material>::load(istream in) -> async::task<renderer::Material>
	{
		auto load_tex = [&](auto&& id, asset::AID placeholder) {
			return _assets.load<graphic::Texture_2D>(id.empty() ? std::move(placeholder)
			                                                    : asset::AID("tex"_strid, id));
		};

		auto desc_set =
		        _descriptor_set_pool.create_descriptor(_descriptor_set_layout, renderer::material_textures);

		if(in.aid().type() == "tex"_strid) {
			// single texture material
			auto albedo   = _assets.load_stream<graphic::Texture_2D>(std::move(in));
			auto normal   = _assets.load<graphic::Texture_2D>("tex:default_normal"_aid);
			auto brdf     = _assets.load<graphic::Texture_2D>("tex:default_brdf"_aid);
			auto emission = _assets.load<graphic::Texture_2D>("tex:placeholder"_aid);
			auto uniforms = create_material_uniform_buffer(_device,
			                                               _queue_family,
			                                               glm::vec4{1, 1, 1, 1},
			                                               glm::vec4(1, 1, 1, 1),
			                                               0.5f,
			                                               0.f,
			                                               1.f,
			                                               false);

			auto all_loaded = async::when_all(albedo.internal_task(),
			                                  normal.internal_task(),
			                                  brdf.internal_task(),
			                                  emission.internal_task(),
			                                  uniforms.transfer_task());
			using Task_type = decltype(all_loaded)::result_type;

			return all_loaded.then([=, desc_set = std::move(desc_set), uniforms = std::move(uniforms)](
			                               const Task_type&) mutable {
				return renderer::Material(_device,
				                          std::move(desc_set),
				                          _sampler,
				                          albedo,
				                          normal,
				                          brdf,
				                          emission,
				                          std::move(uniforms),
				                          true,
				                          false,
				                          false,
				                          false,
				                          "default"_strid,
				                          glm::vec4{1, 1, 1, 1},
				                          glm::vec4(1, 1, 1, 1),
				                          0.5f,
				                          0.f,
				                          1.f);
			});
		}

		auto data = Loader<renderer::Material_data>::load(std::move(in));

		auto sub_id = data.substance_id;

		auto albedo   = load_tex(data.albedo_aid, "tex:placeholder"_aid);
		auto normal   = load_tex(data.normal_aid, "tex:default_normal"_aid);
		auto brdf     = load_tex(data.brdf_aid, "tex:default_brdf"_aid);
		auto emission = load_tex(data.emission_aid, "tex:placeholder"_aid);
		auto uniforms = create_material_uniform_buffer(_device,
		                                               _queue_family,
		                                               data.albedo_color,
		                                               data.emission_color,
		                                               data.roughness,
		                                               data.metallic,
		                                               data.refraction,
		                                               !data.normal_aid.empty());

		auto all_loaded = async::when_all(albedo.internal_task(),
		                                  normal.internal_task(),
		                                  brdf.internal_task(),
		                                  emission.internal_task(),
		                                  uniforms.transfer_task());
		using Task_type = decltype(all_loaded)::result_type;

		return all_loaded.then([=, desc_set = std::move(desc_set), uniforms = std::move(uniforms)](
		                               const Task_type&) mutable {
			return renderer::Material(_device,
			                          std::move(desc_set),
			                          _sampler,
			                          albedo,
			                          normal,
			                          brdf,
			                          emission,
			                          std::move(uniforms),
			                          !data.albedo_aid.empty(),
			                          !data.normal_aid.empty(),
			                          !data.brdf_aid.empty(),
			                          !data.emission_aid.empty(),
			                          sub_id,
			                          data.albedo_color,
			                          data.emission_color,
			                          data.roughness,
			                          data.metallic,
			                          data.refraction);
		});
	}

	namespace {
		template <typename T>
		void read(std::istream& in, T& value)
		{
			static_assert(!std::is_pointer<T>::value,
			              "T is a pointer. That is DEFINITLY not what you wanted!");
			in.read(reinterpret_cast<char*>(&value), sizeof(T));
		}

		template <typename T>
		void read(std::istream& in, std::vector<T>& value)
		{
			static_assert(!std::is_pointer<T>::value,
			              "T is a pointer. That is DEFINITLY not what you wanted!");

			in.read(reinterpret_cast<char*>(value.data()), value.size() * sizeof(T));
		}
	} // namespace

	auto Loader<renderer::Model>::load(istream in) -> async::task<renderer::Model>
	{

		auto header = renderer::Model_file_header{};
		read(in, header);

		if(header.type_tag != renderer::Model_file_header::type_tag_value) {
			MIRRAGE_FAIL("The loaded file \"" << in.aid().str() << "\" is not a valid model file!");
		}

		if(header.version != renderer::Model_file_header::version_value) {
			MIRRAGE_FAIL("The loaded model file \""
			             << in.aid().str() << "\" is not compatible with this version of the application!");
		}

		auto rigged     = (header.flags & 1) != 0;
		auto bone_count = std::int32_t(0);

		if(rigged) {
			read(in, bone_count);
		}

		// load sub meshes and materials
		auto sub_meshes = std::vector<renderer::Sub_mesh>();
		sub_meshes.reserve(header.submesh_count);
		auto material_load_tasks = std::vector<async::shared_task<renderer::Material>>();
		material_load_tasks.reserve(header.submesh_count);

		for(auto i : util::range(header.submesh_count)) {
			(void) i;

			sub_meshes.emplace_back();
			auto& sub_mesh = sub_meshes.back();
			read(in, sub_mesh.index_offset);
			read(in, sub_mesh.index_count);
			read(in, sub_mesh.bounding_sphere_radius);
			read(in, sub_mesh.bounding_sphere_offset.x);
			read(in, sub_mesh.bounding_sphere_offset.y);
			read(in, sub_mesh.bounding_sphere_offset.z);

			auto material_id_length = std::uint32_t(0);
			read(in, material_id_length);

			auto material_id = std::string();
			material_id.resize(material_id_length);
			in.read(material_id.data(), material_id_length);

			sub_mesh.material = _assets.load<renderer::Material>(asset::AID("mat"_strid, material_id));
			material_load_tasks.emplace_back(sub_mesh.material.internal_task());
		}

		// transfer mesh data to gpu
		auto mesh = graphic::Mesh(
		        _device,
		        _owner_qfamily,
		        header.vertex_size,
		        header.index_size,
		        [&](char* dest) { in.read_direct(dest, header.vertex_size); },
		        [&](char* dest) { in.read_direct(dest, header.index_size); });

		auto footer = std::uint32_t(0);
		read(in, footer);

		if(footer != renderer::Model_file_header::type_tag_value) {
			LOG(plog::warning) << "Invalid footer in model file \"" << in.aid().str()
			                   << "\"! The file is probably corrupted!";
		}

		auto all_materials_loaded = async::when_all(material_load_tasks);
		auto all_loaded = async::when_all(all_materials_loaded, mesh.internal_buffer().transfer_task());
		using Task_type = decltype(all_loaded)::result_type;

		auto bounding_sphere_offset = glm::vec3(header.bounding_sphere_offset_x,
		                                        header.bounding_sphere_offset_y,
		                                        header.bounding_sphere_offset_z);
		return all_loaded.then([model = renderer::Model(std::move(mesh),
		                                                std::move(sub_meshes),
		                                                header.bounding_sphere_radius,
		                                                bounding_sphere_offset,
		                                                rigged,
		                                                bone_count,
		                                                in.aid())](const Task_type&) mutable {
			// force init of materials
			for(auto& sm : model.sub_meshes())
				sm.material.get_blocking();

			return std::move(model);
		});
	}

} // namespace mirrage::asset
