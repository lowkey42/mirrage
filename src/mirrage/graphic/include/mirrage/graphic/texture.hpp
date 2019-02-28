#pragma once

#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/transfer_manager.hpp>

#include <mirrage/asset/asset_manager.hpp>

#include <gsl/gsl>
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>


namespace mirrage::graphic {

	class Device;

	namespace detail {
		auto clamp_mip_levels(std::int32_t width, std::int32_t height, std::int32_t mipmaps) -> std::int32_t;

		class Base_texture {
		  public:
			Base_texture(Base_texture&& rhs) noexcept;
			Base_texture& operator=(Base_texture&& rhs) noexcept
			{
				_image      = std::move(rhs._image);
				_image_view = std::move(rhs._image_view);
				return *this;
			}

			auto view() const noexcept { return *_image_view; }
			auto image() const noexcept { return _image.image(); }

			auto width() const noexcept { return _image.width(); }
			auto height() const noexcept { return _image.height(); }
			auto depth() const noexcept { return _image.depth(); }
			auto layers() const noexcept { return _image.layers(); }

			auto width(std::int32_t level) const noexcept { return this->width() / (1 << level); }
			auto height(std::int32_t level) const noexcept { return this->height() / (1 << level); }

		  protected:
			// construct as render target
			Base_texture(Device&,
			             Image_type type,
			             Image_dimensions,
			             std::int32_t mip_levels,
			             vk::Format,
			             vk::ImageUsageFlags,
			             vk::ImageAspectFlags,
			             bool dedicated);

			// construct from loaded data
			Base_texture(Device&, Static_image image, vk::Format);

			// construct as sampled image + load data
			Base_texture(Device&,
			             Image_type type,
			             Image_dimensions,
			             bool generate_mipmaps,
			             vk::Format,
			             gsl::span<const std::uint8_t> data,
			             std::uint32_t                 owner_qfamily);

			virtual ~Base_texture() = default;

		  private:
			Static_image        _image;
			vk::UniqueImageView _image_view;
		};

		extern auto format_from_channels(Device& device, std::int32_t channels, bool srgb) -> vk::Format;
		extern auto build_mip_views(Device&              device,
		                            std::int32_t         mip_levels,
		                            vk::Image            image,
		                            vk::Format           format,
		                            vk::ImageAspectFlags aspects) -> std::vector<vk::UniqueImageView>;
	} // namespace detail

	template <Image_type Type>
	class Texture : public detail::Base_texture {
	  protected:
		Texture(Device&              device,
		        Image_type           type,
		        Image_dimensions     sim,
		        std::int32_t         mip_levels,
		        vk::Format           format,
		        vk::ImageUsageFlags  usage,
		        vk::ImageAspectFlags aspect,
		        bool                 dedicated)
		  : Base_texture(device, type, sim, mip_levels, format, usage, aspect, dedicated)
		{
		}

	  public:
		Texture(Device& device, Static_image image, vk::Format format)
		  : Base_texture(device, std::move(image), format)
		{
		}
		Texture(Device&                  device,
		        Image_dimensions_t<Type> dim,
		        bool                     generate_mipmaps,
		        vk::Format               format,
		        gsl::span<gsl::byte>     data,
		        std::uint32_t            owner_qfamily)
		  : Base_texture(device, Type, dim, generate_mipmaps, format, data, owner_qfamily)
		{
		}
		Texture(Device&                       device,
		        Image_dimensions_t<Type>      dim,
		        bool                          generate_mipmaps,
		        std::int32_t                  channels,
		        bool                          srgb,
		        gsl::span<const std::uint8_t> data,
		        std::uint32_t                 owner_qfamily)
		  : Base_texture(device,
		                 Type,
		                 dim,
		                 generate_mipmaps,
		                 detail::format_from_channels(device, channels, srgb),
		                 data,
		                 owner_qfamily)
		{
		}
	};
	template <Image_type Type>
	class Render_target : public Texture<Type> {
	  public:
		Render_target(Device&                  device,
		              Image_dimensions_t<Type> dim,
		              std::int32_t             mip_levels,
		              vk::Format               format,
		              vk::ImageUsageFlags      usage,
		              vk::ImageAspectFlags     aspects)
		  : Texture<Type>(device, Type, dim, mip_levels, format, usage, aspects, true)
		  , _single_mip_level_views(
		            detail::build_mip_views(device,
		                                    detail::clamp_mip_levels(dim.width, dim.height, mip_levels),
		                                    this->image(),
		                                    format,
		                                    aspects))
		{
		}
		Render_target(Render_target&& rhs) noexcept
		  : Texture<Type>(std::move(rhs)), _single_mip_level_views(std::move(rhs._single_mip_level_views))
		{
		}

		Render_target& operator=(Render_target&&) = default;

		using Texture<Type>::width;
		using Texture<Type>::height;
		using Texture<Type>::view;

		auto view(std::int32_t level) const noexcept { return *_single_mip_level_views.at(level); }
		auto mip_levels() const noexcept { return gsl::narrow<std::int32_t>(_single_mip_level_views.size()); }

	  private:
		std::vector<vk::UniqueImageView> _single_mip_level_views;
	};

	using Texture_1D             = Texture<Image_type::single_1d>;
	using Render_target_1D       = Render_target<Image_type::single_1d>;
	using Texture_2D             = Texture<Image_type::single_2d>;
	using Render_target_2D       = Render_target<Image_type::single_2d>;
	using Texture_2D_array       = Texture<Image_type::array_2d>;
	using Render_target_2D_array = Render_target<Image_type::array_2d>;
	using Texture_cube           = Texture<Image_type::cubemap>;
	using Render_target_cube     = Render_target<Image_type::cubemap>;

	using Texture_ptr = asset::Ptr<Texture_2D>;

	namespace detail {
		extern auto load_image_data(Device& device, std::uint32_t owner_qfamily, asset::istream)
		        -> std::tuple<Static_image, vk::Format, Image_type>;
	}
} // namespace mirrage::graphic


namespace mirrage::asset {

	template <graphic::Image_type Type>
	struct Loader<graphic::Texture<Type>> {
	  public:
		Loader(graphic::Device& device, std::uint32_t owner_qfamily)
		  : _device(device), _owner_qfamily(owner_qfamily)
		{
		}

		auto load(istream in)
		{
			auto data = graphic::detail::load_image_data(_device, _owner_qfamily, std::move(in));
			auto&& [image, format, real_type] = data;
			(void) format;

			if(real_type != Type)
				throw std::system_error(asset::Asset_error::loading_failed,
				                        "Requested image-type doesn't match type read from file: "
				                                + std::to_string(static_cast<int>(Type))
				                                + " != " + std::to_string(static_cast<int>(real_type)));

			return image.transfer_task().then([this, data{std::move(data)}]() mutable {
				return graphic::Texture<Type>{_device, std::move(std::get<0>(data)), std::get<1>(data)};
			});
		}
		void save(ostream, const graphic::Texture<Type>&) { MIRRAGE_FAIL("Save of textures not supported!"); }

	  private:
		graphic::Device& _device;
		std::uint32_t    _owner_qfamily;
	};

} // namespace mirrage::asset
