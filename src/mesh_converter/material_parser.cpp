#define BUILD_SERIALIZER
#include <sf2/sf2.hpp>

#include "material_parser.hpp"

#include <mirrage/renderer/model.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <gli/gli.hpp>
#include <gli/load.hpp>
#include <gli/convert.hpp>
#include <gli/generate_mipmaps.hpp>
#include <stb_image.h>


namespace lux {

	namespace {
		constexpr auto albedo_texture_type       = aiTextureType_DIFFUSE;
		constexpr auto metallic_texture_type     = aiTextureType_AMBIENT;
		constexpr auto roughness_texture_type    = aiTextureType_SHININESS;
		constexpr auto normal_texture_type       = aiTextureType_HEIGHT;
		constexpr auto substamce_id_texture_type = aiTextureType_OPACITY;

		auto get_texture_name(const aiMaterial& material, aiTextureType type) -> util::maybe<std::string> {
			auto count = material.GetTextureCount(type);
			if(count==0)
				return util::nothing;

			auto str = aiString();
			material.GetTexture(type, 0, &str);

			return std::string(str.C_Str());
		}

		auto load_texture2d(const std::string path, bool srgb=true) -> gli::texture2d {
			int width = 0;
			int height = 0;
			auto data = stbi_load(path.c_str(), &width, &height, nullptr, 4);
			INVARIANT(data, "Texture '"<<path<<"' couldn't be loaded!");

			ON_EXIT {
				stbi_image_free(data);
			};

			auto mip_levels = std::floor(std::log2(std::min(width, height))) + 1;
			mip_levels = 1;

			auto texture = gli::texture2d(srgb ? gli::FORMAT_RGBA8_SRGB_PACK8
			                                   : gli::FORMAT_RGBA8_UNORM_PACK8,
			                              gli::texture::extent_type(width, height, 1),
			                              mip_levels);

			INVARIANT(int(texture.size())>=width*height*4, "Sizes don't match");
			std::memcpy(texture.data(), data, width*height*4);

			return texture;
		}

		void store_texture(gli::texture2d texture, const std::string& ouput,
		                   bool srgb = true) {
			texture = gli::generate_mipmaps(texture, gli::FILTER_LINEAR);

			// TODO: compression (DXTx/BCx/...). No support in gli and a sparse library support in general

			gli::save_ktx(texture, ouput);
		}
	}

	bool convert_material(const std::string& name, const aiMaterial& material,
	                      const std::string& base_dir, const std::string& output) {

		auto texture_dir = output + "/textures/";

		auto substance_id = util::Str_id(get_texture_name(material, substamce_id_texture_type)
		                                 .get_or_other("default"));

		// load and combine textures
		auto albedo_name_mb = get_texture_name(material, albedo_texture_type);
		if(albedo_name_mb.is_nothing())
			return false;

		auto albedo_name = base_dir + "/" + albedo_name_mb.get_or_throw();

		auto albedo = load_texture2d(albedo_name);
		INVARIANT(!albedo.empty(), "Couldn't load texture: " << albedo_name);
		albedo_name = name + "_albedo.ktx";
		store_texture(albedo, texture_dir + albedo_name);

		auto material_file = renderer::Material_data{};
		material_file.substance_id = substance_id;
		material_file.albedo_aid = albedo_name;

		switch(substance_id) {
			case "emissive"_strid: // TODO: anything else required?
				break;

			case "default"_strid:
			default:
				auto metallic  = load_texture2d(base_dir + "/" + get_texture_name(material, metallic_texture_type)
				                                .get_or_throw(), false);
				auto roughness = load_texture2d(base_dir + "/" + get_texture_name(material, roughness_texture_type)
				                                .get_or_throw(), false);
				auto normal    = load_texture2d(base_dir + "/" + get_texture_name(material, normal_texture_type)
				                                .get_or_throw(), false);

				auto metallic_data  = reinterpret_cast<gli::u8vec4*>(metallic.data(0, 0, 0));
				auto roughness_data = reinterpret_cast<gli::u8vec4*>(roughness.data(0, 0, 0));
				auto normal_data    = reinterpret_cast<gli::u8vec4*>(normal.data(0, 0, 0));

				auto pixels = static_cast<std::size_t>(metallic.extent().x)
				            * static_cast<std::size_t>(metallic.extent().y);
				for(auto i = std::size_t(0); i<pixels; i++) {
					auto& n = normal_data[i];
					auto fn = glm::vec3(n.x, n.y, n.z)/255.f;
					fn = glm::normalize(fn*2.f-1.f) / 2.f + 0.5f;
					n.r = static_cast<std::uint8_t>(glm::clamp(fn.x * 255.f, 0.f, 255.f));
					n.g = static_cast<std::uint8_t>(glm::clamp(fn.y * 255.f, 0.f, 255.f));
					n.b = roughness_data[i].r;
					n.a = metallic_data[i].g;
				}

				/*
				auto mat_data = gli::texture2d(gli::FORMAT_RGBA8_UNORM_PACK8,
				                               metallic.extent(), metallic.max_level());


				gli::transform<gli::u8vec4>(mat_data, normal, roughness, [](const auto& a, const auto& b) {
					auto n = glm::normalize(glm::vec3(a.x, a.y, a.z)/255.f*2.f-1.f) / 2.f + 0.5f * 255.f;
					return gli::u8vec4(n.r, n.g, b.r, 0);
				});
				gli::transform<gli::u8vec4>(mat_data, mat_data, metallic, [](const auto& a, const auto& b) {
					return gli::u8vec4(a.r, a.g, a.b, b.r);
				});
				*/

				material_file.mat_data_aid = name + "_mat_data.ktx";

				store_texture(normal, texture_dir + material_file.mat_data_aid);

				break;
		}

		// store results
		auto filename = output + "/materials/" + name + ".msf";
		auto file = std::ofstream(filename, std::ostream::trunc);
		sf2::serialize_json(file, material_file);

		return true;
	}

}
