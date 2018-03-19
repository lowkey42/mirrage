#include <mirrage/graphic/texture.hpp>

#include "ktx_parser.hpp"

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/device.hpp>

#include <memory>


namespace mirrage::graphic::detail {

	namespace {
		auto vk_type(Image_type type) {
			switch(type) {
				case Image_type::array_1d:
				case Image_type::single_1d: return vk::ImageType::e1D;

				case Image_type::cubemap:
				case Image_type::array_cubemap:
				case Image_type::array_2d:
				case Image_type::single_2d: return vk::ImageType::e2D;

				case Image_type::array_3d:
				case Image_type::single_3d: return vk::ImageType::e3D;
			}

			MIRRAGE_FAIL("Unreachable");
		}

		auto vk_view_type(Image_type type) {
			switch(type) {
				case Image_type::single_1d: return vk::ImageViewType::e1D;
				case Image_type::array_1d: return vk::ImageViewType::e1DArray;

				case Image_type::cubemap: return vk::ImageViewType::eCube;
				case Image_type::array_cubemap: return vk::ImageViewType::eCubeArray;

				case Image_type::single_2d: return vk::ImageViewType::e2D;
				case Image_type::array_2d: return vk::ImageViewType::e2DArray;

				case Image_type::single_3d: return vk::ImageViewType::e3D;
				case Image_type::array_3d: MIRRAGE_FAIL("3D ImageViews don't seem to be supported!");
			}

			MIRRAGE_FAIL("Unreachable");
		}
	} // namespace


	auto clamp_mip_levels(std::uint32_t width, std::uint32_t height, std::uint32_t mipmaps) -> std::uint32_t {
		if(mipmaps <= 0)
			mipmaps = 9999;

		mipmaps = glm::clamp<std::uint32_t>(
		        mipmaps, 1, std::uint32_t(std::floor(std::log2(std::min(width, height))) + 1));
		return mipmaps;
	}

	auto format_from_channels(Device& device, std::uint32_t channels, bool srgb) -> vk::Format {
		auto format = util::maybe<vk::Format>::nothing();

		switch(channels) {
			case 1: format = srgb ? device.get_texture_sr_format() : device.get_texture_r_format(); break;

			case 2: format = srgb ? device.get_texture_srg_format() : device.get_texture_rg_format(); break;

			case 3: format = srgb ? device.get_texture_srgb_format() : device.get_texture_rgb_format(); break;
			case 4:
				format = srgb ? device.get_texture_srgba_format() : device.get_texture_rgba_format();
				break;
		}

		return format.get_or_throw(
		        "No supported ", srgb ? "srgb" : "", " texture format for ", channels, " channels.");
	}

	Base_texture::Base_texture(Device&              device,
	                           Image_type           type,
	                           Image_dimensions     dim,
	                           std::uint32_t        mip_levels,
	                           vk::Format           format,
	                           vk::ImageUsageFlags  usage,
	                           vk::ImageAspectFlags aspects,
	                           bool                 dedicated)
	  : _image(device.create_image(vk::ImageCreateInfo{vk::ImageCreateFlagBits{},
	                                                   vk_type(type),
	                                                   format,
	                                                   vk::Extent3D{dim.width, dim.height, dim.depth},
	                                                   clamp_mip_levels(dim.width, dim.height, mip_levels),
	                                                   dim.layers,
	                                                   vk::SampleCountFlagBits::e1,
	                                                   vk::ImageTiling::eOptimal,
	                                                   usage},
	                               false,
	                               Memory_lifetime::normal,
	                               dedicated),
	           clamp_mip_levels(dim.width, dim.height, mip_levels),
	           false,
	           dim)
	  , _image_view(device.create_image_view(_image.image(),
	                                         format,
	                                         0,
	                                         _image.mip_level_count(),
	                                         0,
	                                         dim.layers,
	                                         aspects,
	                                         vk_view_type(type))) {}

	Base_texture::Base_texture(Device&                       device,
	                           Image_type                    type,
	                           Image_dimensions              dim,
	                           bool                          generate_mipmaps,
	                           vk::Format                    format,
	                           gsl::span<const std::uint8_t> data,
	                           std::uint32_t                 owner_qfamily)
	  : _image(device.transfer().upload_image(vk_type(type),
	                                          owner_qfamily,
	                                          dim,
	                                          format,
	                                          generate_mipmaps ? 0 : 1,
	                                          data.size_bytes() + 4,
	                                          [&](char* dest) {
		                                          *reinterpret_cast<std::uint32_t*>(dest) = data.size_bytes();
		                                          dest += 4;
		                                          std::memcpy(dest, data.data(), data.size_bytes());
	                                          }))
	  , _image_view(device.create_image_view(image(),
	                                         format,
	                                         0,
	                                         _image.mip_level_count(),
	                                         0,
	                                         dim.layers,
	                                         vk::ImageAspectFlagBits::eColor,
	                                         vk_view_type(type))) {}

	Base_texture::Base_texture(Device& device, Static_image image, vk::Format format)
	  : _image(std::move(image))
	  , _image_view(device.create_image_view(
	            this->image(), format, 0, _image.mip_level_count(), 0, _image.layers())) {}

	auto build_mip_views(Device&              device,
	                     std::uint32_t        mip_levels,
	                     std::uint32_t        array_layers,
	                     vk::Image            image,
	                     vk::Format           format,
	                     Image_type           type,
	                     vk::ImageAspectFlags aspects) -> std::vector<vk::UniqueImageView> {
		auto views = std::vector<vk::UniqueImageView>();
		views.reserve(mip_levels);

		for(auto i : util::range(mip_levels)) {
			views.emplace_back(device.create_image_view(
			        image, format, i, 1, 0, array_layers, aspects, vk_view_type(type)));
		}

		return views;
	}

	auto load_image_data(Device& device, std::uint32_t owner_qfamily, asset::istream in)
	        -> std::tuple<Static_image, vk::Format, Image_type> {
		auto header     = parse_header(in, in.aid().str());
		auto dimensions = Image_dimensions(header.width, header.height, header.depth, header.layers);

		auto image = device.transfer().upload_image(vk_type(header.type),
		                                            owner_qfamily,
		                                            dimensions,
		                                            header.format,
		                                            header.mip_levels,
		                                            header.size,
		                                            [&](char* dest) { in.read_direct(dest, header.size); });

		return {std::move(image), header.format, header.type};
	}

	auto load_image_data(Device&                    device,
	                     std::uint32_t              owner_qfamily,
	                     Image_type                 type,
	                     Image_dimensions           dim,
	                     std::uint32_t              texel_size,
	                     vk::Format                 format,
	                     std::function<void(char*)> write_data) -> Static_image {

		auto size = dim.width * dim.height * dim.depth * dim.layers * texel_size;

		return device.transfer().upload_image(vk_type(type), owner_qfamily, dim, format, 1, size, write_data);
	}

} // namespace mirrage::graphic::detail
