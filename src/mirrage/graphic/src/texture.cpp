#include <mirrage/graphic/texture.hpp>

#include "ktx_parser.hpp"

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/device.hpp>

#include <mirrage/utils/ranges.hpp>

#include <memory>


namespace mirrage::graphic::detail {

	namespace {
		auto vk_type(Image_type type)
		{
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
	} // namespace


	auto clamp_mip_levels(std::int32_t width, std::int32_t height, std::int32_t mipmaps) -> std::int32_t
	{
		if(mipmaps <= 0)
			mipmaps = 9999;

		mipmaps = glm::clamp<std::int32_t>(
		        mipmaps, 1, static_cast<std::int32_t>(std::floor(std::log2(std::min(width, height)))) + 1);
		return mipmaps;
	}

	auto format_from_channels(Device& device, std::int32_t channels, bool srgb) -> vk::Format
	{
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


	Base_texture::Base_texture(Base_texture&& rhs) noexcept
	  : _image(std::move(rhs._image)), _image_view(std::move(rhs._image_view))
	{
	}

	Base_texture::Base_texture(Device&              device,
	                           Image_type           type,
	                           Image_dimensions     dim,
	                           std::int32_t         mip_levels,
	                           vk::Format           format,
	                           vk::ImageUsageFlags  usage,
	                           vk::ImageAspectFlags aspects,
	                           bool                 dedicated)
	  : _image(device.create_image(vk::ImageCreateInfo{vk::ImageCreateFlagBits{},
	                                                   vk_type(type),
	                                                   format,
	                                                   vk::Extent3D{gsl::narrow<std::uint32_t>(dim.width),
	                                                                gsl::narrow<std::uint32_t>(dim.height),
	                                                                gsl::narrow<std::uint32_t>(dim.depth)},
	                                                   gsl::narrow<std::uint32_t>(clamp_mip_levels(
	                                                           dim.width, dim.height, mip_levels)),
	                                                   gsl::narrow<std::uint32_t>(dim.layers),
	                                                   vk::SampleCountFlagBits::e1,
	                                                   vk::ImageTiling::eOptimal,
	                                                   usage},
	                               false,
	                               Memory_lifetime::normal,
	                               dedicated),
	           clamp_mip_levels(dim.width, dim.height, mip_levels),
	           false,
	           dim)
	  , _image_view(device.create_image_view(
	            _image.image(), format, 0, gsl::narrow<std::uint32_t>(_image.mip_level_count()), aspects))
	{
	}

	Base_texture::Base_texture(Device&                       device,
	                           Image_type                    type,
	                           Image_dimensions              dim,
	                           bool                          generate_mipmaps,
	                           vk::Format                    format,
	                           gsl::span<const std::uint8_t> data,
	                           std::uint32_t                 owner_qfamily)
	  : _image(device.transfer().upload_image(
	          vk_type(type),
	          owner_qfamily,
	          dim,
	          format,
	          generate_mipmaps ? 0 : 1,
	          gsl::narrow<std::int32_t>(data.size_bytes() + 4),
	          [&, idx = std::uint32_t(0)](char* dest, std::uint32_t size) mutable {
		          if(idx == 0) {
			          MIRRAGE_INVARIANT(size >= 4, "unexpected read of less than 4 byte");
			          new(dest) std::uint32_t(gsl::narrow<std::uint32_t>(data.size_bytes()));
			          dest += 4;
			          idx += 4;
			          size -= 4;
		          }
		          if(size >= 4) {
			          std::memcpy(dest, data.data() + (idx - 4), size);
		          }
	          }))
	  , _image_view(device.create_image_view(
	            image(), format, 0, gsl::narrow<std::uint32_t>(_image.mip_level_count())))
	{
	}

	Base_texture::Base_texture(Device& device, Static_image image, vk::Format format)
	  : _image(std::move(image))
	  , _image_view(device.create_image_view(
	            this->image(), format, 0, gsl::narrow<std::uint32_t>(_image.mip_level_count())))
	{
	}

	auto build_mip_views(Device&              device,
	                     std::int32_t         mip_levels,
	                     vk::Image            image,
	                     vk::Format           format,
	                     vk::ImageAspectFlags aspects) -> std::vector<vk::UniqueImageView>
	{
		auto views = std::vector<vk::UniqueImageView>();
		views.reserve(gsl::narrow<std::size_t>(mip_levels));

		for(auto i : util::range(mip_levels)) {
			views.emplace_back(device.create_image_view(image, format, i, 1, aspects));
		}

		return views;
	}

	auto load_image_data(Device& device, std::uint32_t owner_qfamily, asset::istream in)
	        -> std::tuple<Static_image, vk::Format, Image_type>
	{
		auto header     = parse_header(in, in.aid().str());
		auto dimensions = Image_dimensions(gsl::narrow<std::int32_t>(header.width),
		                                   gsl::narrow<std::int32_t>(header.height),
		                                   gsl::narrow<std::int32_t>(header.depth),
		                                   gsl::narrow<std::int32_t>(header.layers));

		auto image = device.transfer().upload_image(
		        vk_type(header.type),
		        owner_qfamily,
		        dimensions,
		        header.format,
		        gsl::narrow<std::int32_t>(header.mip_levels),
		        gsl::narrow<std::int32_t>(header.size),
		        [&](char* dest, std::uint32_t size) { in.read_direct(dest, size); });

		return {std::move(image), header.format, header.type};
	}

} // namespace mirrage::graphic::detail
