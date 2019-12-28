#define BUILD_SERIALIZER
#include <sf2/sf2.hpp>

#include "filesystem.hpp"
#include "material_parser.hpp"

#include <mirrage/renderer/model.hpp>
#include <mirrage/utils/min_max.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <crnlib.h>
#include <stb_image.h>
#include <glm/gtc/round.hpp>

#ifndef __clang_analyzer__
#include <async++.h>
#endif

#include <fstream>
#include <iostream>
#include <string>


using namespace std::string_literals;

namespace mirrage {

	namespace {
		constexpr char          padding_data[] = {0x0, 0x0, 0x0, 0x0};
		constexpr unsigned char type_tag[]     = {
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

		using rg8    = glm::vec<2, std::uint8_t, glm::highp>;
		using rgba8  = glm::vec<4, std::uint8_t, glm::highp>;
		using rgba32 = glm::vec<4, float, glm::highp>;

		template <typename T>
		struct Image_data {
			using Pixels = std::vector<T>;

			std::int_fast32_t   width;
			std::int_fast32_t   height;
			std::vector<Pixels> mip_levels;

			Image_data() = default;
			Image_data(std::int_fast32_t width, std::int_fast32_t height)
			  : width(width)
			  , height(height)
			  , mip_levels(std::size_t(util::max(1, std::floor(std::log2(std::min(width, height))) - 1)))
			{
				for(auto i = 0u; i < mip_levels.size(); i++) {
					auto width  = std::max(std::int_fast32_t(1), this->width >> i);
					auto height = std::max(std::int_fast32_t(1), this->height >> i);

					mip_levels[i].resize(std::size_t(width * height));
				}
			}

			auto& pixel(std::int_fast32_t level, std::int_fast32_t x, std::int_fast32_t y)
			{
				return mip_levels.at(std::size_t(level)).at(std::size_t(y * (width >> level) + x));
			}

			template <typename F>
			void foreach(F&& f)
			{
				for(std::uint32_t i = 0u; i < mip_levels.size(); i++) {
					auto width  = std::max(std::int_fast32_t(1), this->width >> i);
					auto height = std::max(std::int_fast32_t(1), this->height >> i);
					for(std::uint32_t y = 0; y < height; y++) {
						for(std::uint32_t x = 0; x < width; x++) {
							f(pixel(i, x, y), i, x, y);
						}
					}
				}
			}
		};

		auto get_texture_name(const aiMaterial& material, aiTextureType type) -> util::maybe<std::string>
		{
			auto count = material.GetTextureCount(type);
			if(count == 0)
				return util::nothing;

			auto str = aiString();
			material.GetTexture(type, 0, &str);

			return std::string(str.C_Str());
		}

		auto load_texture2d(const std::string path, bool srgb = true) -> Image_data<rgba32>
		{
			int  width  = 0;
			int  height = 0;
			auto data   = stbi_load(path.c_str(), &width, &height, nullptr, 4);
			MIRRAGE_INVARIANT(data, "Texture '" << path << "' couldn't be loaded!");

			ON_EXIT { stbi_image_free(data); };

			auto image = Image_data<glm::vec4>(width, height);

			for(std::uint32_t y = 0; y < image.height; y++) {
				for(std::uint32_t x = 0; x < image.width; x++) {
					auto pixel = rgba32{static_cast<float>(data[(y * image.width + x) * 4 + 0]) / 255.f,
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
		void generate_mip_maps(Image_data<T>& image, F&& f)
		{
			for(std::uint32_t i = 1u; i < image.mip_levels.size(); i++) {
				auto width  = util::max(1, image.width >> i);
				auto height = util::max(1, image.height >> i);
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

		auto to_8bit(float v) -> std::uint8_t
		{
			return static_cast<std::uint8_t>(glm::clamp(v * 255.f, 0.f, 255.f));
		}

		enum class Texture_format { s_rgba, rg };

		void store_texture(Image_data<rgba32>   image,
		                   std::string          ouput,
		                   Texture_format       format,
		                   int                  dxt_level,
		                   helper::Progress_ref progress = {})
		{
			static constexpr auto cDXTBlockSize = std::int32_t(4);

			if(format == Texture_format::s_rgba) {
				for(auto& level_data : image.mip_levels) {
					for(auto& pixel : level_data) {
						pixel.r = std::pow(pixel.r, 2.2f);
						pixel.g = std::pow(pixel.g, 2.2f);
						pixel.b = std::pow(pixel.b, 2.2f);
						pixel.a = std::pow(pixel.a, 2.2f);
					}
				}
			}

			auto out = std::ofstream(ouput, std::ofstream::binary);

			const auto crn_format            = format == Texture_format::rg ? cCRNFmtDXN_XY : cCRNFmtDXT5;
			auto       comp_params           = crn_comp_params{};
			comp_params.m_format             = crn_format;
			comp_params.m_dxt_quality        = static_cast<crn_dxt_quality>(dxt_level); //cCRNDXTQualityUber;
			comp_params.m_num_helper_threads = 8;
			comp_params.set_flag(cCRNCompFlagPerceptual, format == Texture_format::s_rgba);
			comp_params.set_flag(cCRNCompFlagDXT1AForTransparency, format == Texture_format::s_rgba);
			auto pContext = crn_create_block_compressor(comp_params);

			out.write(reinterpret_cast<const char*>(&type_tag), sizeof(type_tag));
			auto header                  = ktx_header10{};
			header.Endianness            = 0x04030201;
			header.GLType                = 0;
			header.GLTypeSize            = 1;
			header.GLFormat              = 0;
			header.GLInternalFormat      = 0x8C4F; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
			header.GLBaseInternalFormat  = 0x1908; // GL_RGBA
			header.PixelWidth            = std::uint32_t(image.width);
			header.PixelHeight           = std::uint32_t(image.height);
			header.PixelDepth            = 0;
			header.NumberOfArrayElements = 0;
			header.NumberOfFaces         = 1;
			header.NumberOfMipmapLevels  = std::uint32_t(image.mip_levels.size());
			header.BytesOfKeyValueData   = 0;

			if(format == Texture_format::rg) {
				header.GLInternalFormat     = 0x8DBD; // GL_COMPRESSED_RG_RGTC2
				header.GLBaseInternalFormat = 0x8227; // GL_RG
			}

			out.write(reinterpret_cast<char*>(&header), sizeof(header));

			char       block_data[cDXTBlockSize * cDXTBlockSize * 4];
			crn_uint32 pixels[cDXTBlockSize * cDXTBlockSize];

			auto block_count    = std::size_t(0);
			auto blocks_written = std::size_t(0);
			for(std::uint32_t i = 0; i < image.mip_levels.size(); i++) {
				auto width        = util::max(1, image.width >> i);
				auto height       = util::max(1, image.height >> i);
				auto num_blocks_x = std::size_t((width + cDXTBlockSize - 1) / cDXTBlockSize);
				auto num_blocks_y = std::size_t((height + cDXTBlockSize - 1) / cDXTBlockSize);

				block_count += num_blocks_x * num_blocks_y;
			}

			for(std::uint32_t i = 0; i < image.mip_levels.size(); i++) {
				// calc and write size
				auto width  = util::max(1, image.width >> i);
				auto height = util::max(1, image.height >> i);

				auto num_blocks_x          = std::int32_t((width + cDXTBlockSize - 1) / cDXTBlockSize);
				auto num_blocks_y          = std::int32_t((height + cDXTBlockSize - 1) / cDXTBlockSize);
				auto bytes_per_block       = crn_get_bytes_per_dxt_block(crn_format);
				auto total_compressed_size = std::uint32_t(num_blocks_x * num_blocks_y) * bytes_per_block;

				auto size_padded = glm::ceilMultiple(total_compressed_size, std::uint32_t(4));
				out.write(reinterpret_cast<char*>(&size_padded), sizeof(std::uint32_t));

				// write pixel data
				for(std::int32_t block_y = 0; block_y < num_blocks_y; block_y++) {
					for(std::int32_t block_x = 0; block_x < num_blocks_x; block_x++) {
						// read block from source
						crn_uint32* pDst_pixels = pixels;
						for(int y = 0; y < cDXTBlockSize; y++) {
							auto y_clipped = util::min(height - 1U, (block_y * cDXTBlockSize) + y);
							for(int x = 0; x < cDXTBlockSize; x++) {
								auto x_clipped = util::min(width - 1U, (block_x * cDXTBlockSize) + x);
								auto src       = image.pixel(i, x_clipped, y_clipped);
								*pDst_pixels++ = crn_uint32(to_8bit(src.r)) << 8 * 0
								                 | crn_uint32(to_8bit(src.g)) << 8 * 1
								                 | crn_uint32(to_8bit(src.b)) << 8 * 2
								                 | crn_uint32(to_8bit(src.a)) << 8 * 3;
							}
						}

						crn_compress_block(pContext, pixels, block_data);

						out.write(block_data, std::streamsize(bytes_per_block));
						blocks_written++;
						progress.progress(static_cast<float>(blocks_written) / block_count);
					}
				}

				if(size_padded > total_compressed_size) {
					// padding
					out.write(padding_data, std::streamsize(size_padded - total_compressed_size));
				}
			}

			progress.progress(1.f);
			progress.color(indicators::Color::GREEN);
		}
		template <typename F>
		void store_texture_async(std::string                 output,
		                         Texture_format              format,
		                         int                         dxt_level,
		                         helper::Progress_container& progress,
		                         F&&                         image_src)
		{
			parallel_tasks_started++;
			auto last_slash = output.find_last_of("/");
			auto p          = progress.add(indicators::Color::YELLOW,
                                  last_slash != std::string::npos ? output.substr(last_slash + 1) : output);
			async::spawn([image_src = std::forward<F>(image_src),
			              output    = std::move(output),
			              format,
			              p,
			              dxt_level]() mutable {
				store_texture(std::move(image_src)(), std::move(output), format, dxt_level, p);
				parallel_tasks_done++;
			});
		}

		[[maybe_unused]] void store_texture_async(Image_data<rgba32>          image,
		                                          std::string                 output,
		                                          Texture_format              format,
		                                          int                         dxt_level,
		                                          helper::Progress_container& progress)
		{
			store_texture_async(output, format, dxt_level, progress, [image = std::move(image)]() mutable {
				return std::move(image);
			});
		}

		auto try_find_texture(const std::string&           mat_name,
		                      const aiMaterial&            material,
		                      const Mesh_converted_config& cfg,
		                      Texture_type                 type) -> util::maybe<std::string>
		{
			auto override = util::find_maybe(cfg.material_texture_override, mat_name)
			                        .process([&](auto& override) { return util::find_maybe(override, type); })
			                        .get_or(util::nothing);
			if(override.is_some()) {
				return override.get_or_throw();
			}

			auto tex_mapping = cfg.texture_mappings.find(type);
			if(tex_mapping != cfg.texture_mappings.end()) {
				for(auto& type : tex_mapping->second) {
					auto texture_name = get_texture_name(material, static_cast<aiTextureType>(type));
					if(texture_name.is_some())
						return texture_name.get_or_throw();
				}
			}

			return util::nothing;
		}
		auto find_texture(const std::string&           mat_name,
		                  const aiMaterial&            material,
		                  const Mesh_converted_config& cfg,
		                  Texture_type                 type) -> std::string
		{
			auto tex = try_find_texture(mat_name, material, cfg, type);
			if(tex.is_some())
				return tex.get_or_throw();

			auto name         = sf2::get_enum_info<Texture_type>().name_of(type).str();
			auto texture_name = "default_"s + name + ".png";

			LOG(plog::debug) << "No " << name << " texture for material '" << mat_name
			                 << "'. Trying fallback to '" << texture_name << "'.";

			return texture_name;
		}

		auto resolve_path(const std::string& mat_name, const std::string& base_dir, std::string filename)
		{
			util::trim(filename);

			MIRRAGE_INVARIANT(!filename.empty(), "Texture path in material '" << mat_name << "' is empty");

			if(filename[0] == '*') {
				MIRRAGE_FAIL("Embedded textures are currently not supported. Used by material: " << mat_name);

			} else if(filename[0] != '/') {
				return base_dir + "/" + filename; // relative path

			} else if(exists(filename)) {
				return filename; // valid absolute path, just return it

			} else if(auto pos = filename.find("/TEXTURE/"); pos != std::string::npos) {
				// try to remove invalid directory that blender likes to include for some reason
				return filename.substr(0, pos) + "/" + filename.substr(pos + 9);

			} else {
				// try to find just the filename
				return base_dir + "/" + filename.substr(filename.find_last_of('/') + 1);
			}
		}
	} // namespace

	bool convert_material(const std::string&           name,
	                      const aiMaterial&            material,
	                      const std::string&           base_dir,
	                      const std::string&           output,
	                      const Mesh_converted_config& cfg,
	                      helper::Progress_container&  progress)
	{

		if(cfg.print_material_info) {
			LOG(plog::info) << "Material '" << name << "':";
			for(auto i = int(aiTextureType::aiTextureType_NONE);
			    i < int(aiTextureType::aiTextureType_UNKNOWN);
			    i++) {
				get_texture_name(material, aiTextureType(i)).process([&](auto& texture) {
					LOG(plog::info) << "    " << i << " : " << texture;
				});
			}
		}

		auto texture_dir = output + "/textures/";

		auto material_file         = renderer::Material_data{};
		material_file.substance_id = util::Str_id("default"); // TODO: decide alpha-test / alpha-blend

		// convert albedo
		auto albedo_name =
		        resolve_path(name, base_dir, find_texture(name, material, cfg, Texture_type::albedo));

		auto albedo              = load_texture2d(albedo_name);
		material_file.albedo_aid = name + "_albedo.ktx";

		// determine substance type
		auto uses_alpha_mask  = std::int_fast32_t(0);
		auto uses_alpha_blend = std::int_fast32_t(0);
		albedo.foreach([&](auto& pixel, auto level, auto, auto) {
			if(level == 0) {
				auto mask = pixel.a < 254.f / 255.f;
				uses_alpha_mask += mask ? 1 : 0;
				uses_alpha_blend += mask && pixel.a > 2.f / 255.f ? 1 : 0;
			}
		});
		auto alpha_cutoff = std::clamp<std::int_fast32_t>((albedo.width * albedo.height) / 100, 10, 200);
		if(uses_alpha_blend > alpha_cutoff)
			material_file.substance_id = util::Str_id("alphatest"); // TODO: alphablend
		else if(uses_alpha_mask > alpha_cutoff)
			material_file.substance_id = util::Str_id("alphatest");

		store_texture_async(texture_dir + material_file.albedo_aid,
		                    Texture_format::s_rgba,
		                    cfg.dxt_level,
		                    progress,
		                    [albedo = std::move(albedo)]() mutable {
			                    generate_mip_maps(albedo, [](auto a, auto b, auto c, auto d) {
				                    return (a + b + c + d) / 4.f;
			                    });
			                    return std::move(albedo);
		                    });

		// convert normal
		auto normal_name = find_texture(name, material, cfg, Texture_type::normal);
		auto normal_gen  = [path = resolve_path(name, base_dir, normal_name)]() mutable {
            auto normal = load_texture2d(path, false);
            normal.foreach([&](auto& pixel, auto level, auto x, auto y) {
                // generate mip levels
                if(level > 0) {
                    auto n_00 = normal.pixel(level - 1, x * 2, y * 2);
                    auto n_10 = normal.pixel(level - 1, x * 2 + 1, y * 2);
                    auto n_11 = normal.pixel(level - 1, x * 2 + 1, y * 2 + 1);
                    auto n_01 = normal.pixel(level - 1, x * 2, y * 2 + 1);

                    auto n = glm::normalize(glm::vec3(n_00.r * 2 - 1, n_00.g * 2 - 1, n_00.b * 2 - 1))
                             + glm::normalize(glm::vec3(n_10.r * 2 - 1, n_10.g * 2 - 1, n_10.b * 2 - 1))
                             + glm::normalize(glm::vec3(n_11.r * 2 - 1, n_11.g * 2 - 1, n_11.b * 2 - 1))
                             + glm::normalize(glm::vec3(n_01.r * 2 - 1, n_01.g * 2 - 1, n_01.b * 2 - 1));

                    n       = glm::normalize(n / 4.f);
                    pixel.r = n.x * 0.5f + 0.5f;
                    pixel.g = n.y * 0.5f + 0.5f;
                    pixel.b = n.z * 0.5f + 0.5f;
                    pixel.a = 1;
                }
            });
            return normal;
		};
		material_file.normal_aid = name + "_normal.ktx";
		store_texture_async(texture_dir + material_file.normal_aid,
		                    Texture_format::rg,
		                    cfg.dxt_level,
		                    progress,
		                    normal_gen);

		// convert brdf
		auto metallic_name  = find_texture(name, material, cfg, Texture_type::metalness);
		auto roughness_name = find_texture(name, material, cfg, Texture_type::roughness);
		auto mat_gen        = [metallic_path  = resolve_path(name, base_dir, metallic_name),
                        roughness_path = resolve_path(name, base_dir, roughness_name)]() mutable {
            auto metallic  = load_texture2d(metallic_path, false);
            auto roughness = load_texture2d(roughness_path, false);

            generate_mip_maps(metallic, [](auto a, auto b, auto c, auto d) { return (a + b + c + d) / 4.f; });
            generate_mip_maps(roughness,
                              [](auto a, auto b, auto c, auto d) { return (a + b + c + d) / 4.f; });
            if(roughness.width == metallic.width && roughness.height == metallic.height)
                roughness.foreach([&](auto& pixel, auto level, auto x, auto y) {
                    pixel.g = metallic.pixel(level, x, y).r;
                });
            else {
                auto width_scale  = float(metallic.width) / float(roughness.width);
                auto height_scale = float(metallic.height) / float(roughness.height);
                roughness.foreach([&](auto& pixel, auto level, auto x, auto y) {
                    pixel.g = metallic.pixel(level,
                                             std::int32_t(x * width_scale),
                                             std::int32_t(y * height_scale))
                                      .r;
                });
            }
            return roughness;
		};
		material_file.brdf_aid = name + "_brdf.ktx";
		store_texture_async(
		        texture_dir + material_file.brdf_aid, Texture_format::rg, cfg.dxt_level, progress, mat_gen);

		// convert emissive
		try_find_texture(name, material, cfg, Texture_type::emission).process([&](auto& emission_name) {
			material_file.emission_aid = name + "_emission.ktx";
			store_texture_async(texture_dir + material_file.emission_aid,
			                    Texture_format::rg,
			                    cfg.dxt_level,
			                    progress,
			                    [path = resolve_path(name, base_dir, emission_name)]() mutable {
				                    auto emission = load_texture2d(path, false);
				                    generate_mip_maps(emission, [](auto a, auto b, auto c, auto d) {
					                    return (a + b + c + d) / 4.f;
				                    });
				                    return emission;
			                    });
		});

		// store results
		auto filename = output + "/materials/" + util::to_lower(name) + ".msf";
		auto file     = std::ofstream(filename, std::ostream::trunc);
		sf2::serialize_json(file, material_file);

		return true;
	}

	void convert_texture(const std::string&              input,
	                     const util::maybe<std::string>& name,
	                     const std::string&              output_dir,
	                     bool                            normal_texture,
	                     bool                            srgb,
	                     int                             dxt_level,
	                     helper::Progress_container&     progress)
	{
		auto gen = [input, srgb, normal_texture]() mutable {
			auto data = load_texture2d(input, srgb);
			if(normal_texture) {
				data.foreach([&](auto& pixel, auto level, auto x, auto y) {
					// generate mip levels
					if(level > 0) {
						auto n_00 = data.pixel(level - 1, x * 2, y * 2);
						auto n_10 = data.pixel(level - 1, x * 2 + 1, y * 2);
						auto n_11 = data.pixel(level - 1, x * 2 + 1, y * 2 + 1);
						auto n_01 = data.pixel(level - 1, x * 2, y * 2 + 1);

						auto n = glm::normalize(glm::vec3(n_00.r * 2 - 1, n_00.g * 2 - 1, n_00.b * 2 - 1))
						         + glm::normalize(glm::vec3(n_10.r * 2 - 1, n_10.g * 2 - 1, n_10.b * 2 - 1))
						         + glm::normalize(glm::vec3(n_11.r * 2 - 1, n_11.g * 2 - 1, n_11.b * 2 - 1))
						         + glm::normalize(glm::vec3(n_01.r * 2 - 1, n_01.g * 2 - 1, n_01.b * 2 - 1));

						n       = glm::normalize(n / 4.f);
						pixel.r = n.x * 0.5f + 0.5f;
						pixel.g = n.y * 0.5f + 0.5f;
						pixel.b = n.z * 0.5f + 0.5f;
						pixel.a = 1;
					}
				});

			} else {
				generate_mip_maps(data, [](auto a, auto b, auto c, auto d) { return (a + b + c + d) / 4.f; });
			}

			return data;
		};

		auto input_path = util::split_on_last(input, "/");
		auto input_name = input_path.second.empty() ? input_path.first : input_path.second;
		input_name      = util::split_on_last(input_name, ".").first;

		store_texture_async(output_dir + "/" + name.get_or(util::to_lower(input_name)) + ".ktx",
		                    srgb ? Texture_format::s_rgba : Texture_format::rg,
		                    dxt_level,
		                    progress,
		                    std::move(gen));
	}
} // namespace mirrage
