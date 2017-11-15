#define BUILD_SERIALIZER
#include <sf2/sf2.hpp>

#include "material_parser.hpp"

#include <mirrage/renderer/model.hpp>
#include <mirrage/utils/math.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <stb_image.h>
#include <glm/gtc/round.hpp>

#include <fstream>
#include <iostream>


namespace mirrage {

	namespace {
		constexpr auto albedo_texture_type       = aiTextureType_DIFFUSE;
		constexpr auto metallic_texture_type     = aiTextureType_AMBIENT;
		constexpr auto roughness_texture_type    = aiTextureType_SHININESS;
		constexpr auto normal_texture_type       = aiTextureType_HEIGHT;
		constexpr auto substamce_id_texture_type = aiTextureType_OPACITY;

		constexpr unsigned char type_tag[] = {
		        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

		struct ktx_header10 {
			std::uint32_t Endianness;
			std::uint32_t GLType;
			std::uint32_t GLTypeSize;
			std::uint32_t GLFormat;
			std::uint32_t GLInternalFormat;
			std::uint32_t GLBaseInternalFormat;
			std::uint32_t PixelWidth;
			std::uint32_t PixelHeight;
			std::uint32_t PixelDepth;
			std::uint32_t NumberOfArrayElements;
			std::uint32_t NumberOfFaces;
			std::uint32_t NumberOfMipmapLevels;
			std::uint32_t BytesOfKeyValueData;
		};


		template <typename T>
		struct Pixel {
			T r;
			T g;
			T b;
			T a;
		};

		template <typename T>
		auto operator+(const Pixel<T>& lhs, const Pixel<T>& rhs) {
			return Pixel<T>{lhs.r + rhs.r, lhs.g + rhs.g, lhs.b + rhs.b, lhs.a + rhs.a};
		}

		template <typename T>
		auto operator*(const Pixel<T>& lhs, T rhs) {
			return Pixel<T>{lhs.r * rhs, lhs.g * rhs, lhs.b * rhs, lhs.a * rhs};
		}

		template <typename T>
		auto operator*(T lhs, const Pixel<T>& rhs) {
			return Pixel<T>{lhs * rhs.r, lhs * rhs.g, lhs * rhs.b, lhs * rhs.a};
		}

		template <typename T>
		auto operator/(const Pixel<T>& lhs, T rhs) {
			return Pixel<T>{lhs.r / rhs, lhs.g / rhs, lhs.b / rhs, lhs.a / rhs};
		}


		template <typename T = std::uint8_t>
		struct Image_data {
			using Pixels = std::vector<Pixel<T>>;

			std::uint32_t       width;
			std::uint32_t       height;
			std::vector<Pixels> mip_levels;

			Image_data() = default;
			Image_data(std::uint32_t width, std::uint32_t height)
			  : width(width)
			  , height(height)
			  , mip_levels(std::floor(std::log2(std::min(width, height))) + 1) {
				for(auto i = 0u; i < mip_levels.size(); i++) {
					auto width  = std::max(1u, this->width >> i);
					auto height = std::max(1u, this->height >> i);

					mip_levels[i].resize(width * height);
				}
			}

			auto& pixel(std::uint32_t level, std::uint32_t x, std::uint32_t y) {
				return mip_levels.at(level).at(y * (width >> level) + x);
			}

			template <typename F>
			void foreach(F&& f) {
				for(std::uint32_t i = 0u; i < mip_levels.size(); i++) {
					auto width  = std::max(1u, this->width >> i);
					auto height = std::max(1u, this->height >> i);
					for(std::uint32_t y = 0; y < height; y++) {
						for(std::uint32_t x = 0; x < width; x++) {
							f(pixel(i, x, y), i, x, y);
						}
					}
				}
			}
		};

		auto get_texture_name(const aiMaterial& material, aiTextureType type)
		        -> util::maybe<std::string> {
			auto count = material.GetTextureCount(type);
			if(count == 0)
				return util::nothing;

			auto str = aiString();
			material.GetTexture(type, 0, &str);

			return std::string(str.C_Str());
		}

		auto load_texture2d(const std::string path, bool srgb = true) -> Image_data<float> {
			int  width  = 0;
			int  height = 0;
			auto data   = stbi_load(path.c_str(), &width, &height, nullptr, 4);
			MIRRAGE_INVARIANT(data, "Texture '" << path << "' couldn't be loaded!");

			ON_EXIT { stbi_image_free(data); };

			auto image = Image_data<float>(width, height);

			for(std::uint32_t y = 0; y < image.height; y++) {
				for(std::uint32_t x = 0; x < image.width; x++) {
					auto pixel = Pixel<float>{
					        static_cast<float>(data[(y * image.width + x) * 4 + 0]) / 255.f,
					        static_cast<float>(data[(y * image.width + x) * 4 + 1]) / 255.f,
					        static_cast<float>(data[(y * image.width + x) * 4 + 2]) / 255.f,
					        static_cast<float>(data[(y * image.width + x) * 4 + 3]) / 255.f};

					if(srgb) {
						pixel.r = std::pow(pixel.r, 1.f / 2.2f);
						pixel.g = std::pow(pixel.g, 1.f / 2.2f);
						pixel.b = std::pow(pixel.b, 1.f / 2.2f);
						pixel.a = std::pow(pixel.a, 1.f / 2.2f);
					}

					image.pixel(0, x, y) = pixel;
				}
			}

			return image;
		}

		template <typename T, typename F>
		void generate_mip_maps(Image_data<T>& image, F&& f) {
			for(std::uint32_t i = 1u; i < image.mip_levels.size(); i++) {
				auto width  = std::max(1u, image.width >> i);
				auto height = std::max(1u, image.height >> i);
				for(std::uint32_t y = 0; y < height; y++) {
					for(std::uint32_t x = 0; x < width; x++) {
						image.pixel(i, x, y) = f(image.pixel(i - 1, x * 2, y * 2),
						                         image.pixel(i - 1, x * 2 + 1, y * 2),
						                         image.pixel(i - 1, x * 2 + 1, y * 2 + 1),
						                         image.pixel(i - 1, x * 2, y * 2 + 1));
					}
				}
			}
		}

		void store_texture(Image_data<float>& image, const std::string& ouput, bool srgb = true) {
			if(srgb) {
				for(auto& level_data : image.mip_levels) {
					for(auto& pixel : level_data) {
						pixel.r = std::pow(pixel.r, 2.2f);
						pixel.g = std::pow(pixel.g, 2.2f);
						pixel.b = std::pow(pixel.b, 2.2f);
						pixel.a = std::pow(pixel.a, 2.2f);
					}
				}
			}

			auto rgb8_image = Image_data<std::uint8_t>(image.width, image.height);

			for(std::uint32_t i = 0u; i < image.mip_levels.size(); i++) {
				auto width  = std::max(1u, image.width >> i);
				auto height = std::max(1u, image.height >> i);
				for(std::uint32_t y = 0; y < height; y++) {
					for(std::uint32_t x = 0; x < width; x++) {
						auto  src = image.pixel(i, x, y);
						auto& dst = rgb8_image.pixel(i, x, y);
						dst.r = static_cast<std::uint8_t>(glm::clamp(src.r * 255.f, 0.f, 255.f));
						dst.g = static_cast<std::uint8_t>(glm::clamp(src.g * 255.f, 0.f, 255.f));
						dst.b = static_cast<std::uint8_t>(glm::clamp(src.b * 255.f, 0.f, 255.f));
						dst.a = static_cast<std::uint8_t>(glm::clamp(src.a * 255.f, 0.f, 255.f));
					}
				}
			}


			// TODO: compression (DXTx/BCx/...). No support in gli and a sparse library support in general

			auto out = std::ofstream(ouput, std::ofstream::binary);

			out.write(reinterpret_cast<const char*>(&type_tag), sizeof(type_tag));
			auto header                  = ktx_header10{};
			header.Endianness            = 0x04030201;
			header.GLType                = 0x1401;
			header.GLTypeSize            = 4;
			header.GLFormat              = 0x1908;
			header.GLInternalFormat      = srgb ? 0x8C43 : 0x8058;
			header.GLBaseInternalFormat  = 0x1908;
			header.PixelWidth            = image.width;
			header.PixelHeight           = image.height;
			header.PixelDepth            = 0;
			header.NumberOfArrayElements = 0;
			header.NumberOfFaces         = 1;
			header.NumberOfMipmapLevels  = image.mip_levels.size();
			header.BytesOfKeyValueData   = 0;

			out.write(reinterpret_cast<char*>(&header), sizeof(header));

			for(std::uint32_t i = 0; i < rgb8_image.mip_levels.size(); i++) {
				auto size        = std::uint32_t((image.width >> i) * (image.height >> i) * 4);
				auto size_padded = glm::ceilMultiple(size, std::uint32_t(4));
				out.write(reinterpret_cast<char*>(&size_padded), sizeof(std::uint32_t));

				out.write(reinterpret_cast<char*>(rgb8_image.mip_levels[i].data()), size);
			}
		}
	} // namespace

	bool convert_material(const std::string& name,
	                      const aiMaterial&  material,
	                      const std::string& base_dir,
	                      const std::string& output) {

		auto texture_dir = output + "/textures/";

		auto substance_id = util::Str_id(
		        get_texture_name(material, substamce_id_texture_type).get_or("default"));

		// load and combine textures
		auto albedo_name_mb = get_texture_name(material, albedo_texture_type);
		if(albedo_name_mb.is_nothing())
			return false;

		auto albedo_name = base_dir + "/" + albedo_name_mb.get_or_throw();

		auto albedo = load_texture2d(albedo_name);
		generate_mip_maps(albedo,
		                  [](auto a, auto b, auto c, auto d) { return (a + b + c + d) / 4.f; });
		albedo_name = name + "_albedo.ktx";
		store_texture(albedo, texture_dir + albedo_name);

		auto material_file         = renderer::Material_data{};
		material_file.substance_id = substance_id;
		material_file.albedo_aid   = albedo_name;

		switch(substance_id) {
			case "emissive"_strid: // TODO: anything else required?
				break;

			case "default"_strid:
			default:
				auto metallic = load_texture2d(
				        base_dir + "/"
				                + get_texture_name(material, metallic_texture_type).get_or_throw(),
				        false);
				auto roughness = load_texture2d(
				        base_dir + "/"
				                + get_texture_name(material, roughness_texture_type).get_or_throw(),
				        false);
				auto normal = load_texture2d(
				        base_dir + "/"
				                + get_texture_name(material, normal_texture_type).get_or_throw(),
				        false);

				generate_mip_maps(metallic, [](auto a, auto b, auto c, auto d) {
					return (a + b + c + d) / 4.f;
				});
				generate_mip_maps(roughness, [](auto a, auto b, auto c, auto d) {
					return (a + b + c + d) / 4.f;
				});

				normal.foreach([&](auto& pixel, auto level, auto x, auto y) {
					if(level > 0) {
						auto n_00 = normal.pixel(level - 1, x * 2, y * 2);
						auto n_10 = normal.pixel(level - 1, x * 2 + 1, y * 2);
						auto n_11 = normal.pixel(level - 1, x * 2 + 1, y * 2 + 1);
						auto n_01 = normal.pixel(level - 1, x * 2, y * 2 + 1);

						auto n = glm::normalize(
						                 glm::vec3(n_00.r * 2 - 1, n_00.g * 2 - 1, n_00.b * 2 - 1))
						         + glm::normalize(glm::vec3(
						                   n_10.r * 2 - 1, n_10.g * 2 - 1, n_10.b * 2 - 1))
						         + glm::normalize(glm::vec3(
						                   n_11.r * 2 - 1, n_11.g * 2 - 1, n_11.b * 2 - 1))
						         + glm::normalize(glm::vec3(
						                   n_01.r * 2 - 1, n_01.g * 2 - 1, n_01.b * 2 - 1));

						n       = glm::normalize(n / 4.f);
						pixel.r = n.x * 0.5 + 0.5;
						pixel.g = n.y * 0.5 + 0.5;
						pixel.b = n.z * 0.5 + 0.5;
						pixel.a = 1;
					}
				});

				normal.foreach([&](auto& pixel, auto level, auto x, auto y) {
					pixel.b = roughness.pixel(level, x, y).r;
					pixel.a = metallic.pixel(level, x, y).r;
				});

				material_file.mat_data_aid = name + "_mat_data.ktx";
				store_texture(normal, texture_dir + material_file.mat_data_aid, false);

				break;
		}

		// store results
		auto filename = output + "/materials/" + name + ".msf";
		auto file     = std::ofstream(filename, std::ostream::trunc);
		sf2::serialize_json(file, material_file);

		return true;
	}
} // namespace mirrage
