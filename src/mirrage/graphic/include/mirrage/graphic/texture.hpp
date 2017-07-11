#pragma once

#include <mirrage/graphic/device_memory.hpp>
#include <mirrage/graphic/transfer_manager.hpp>

#include <mirrage/asset/asset_manager.hpp>

#include <vulkan/vulkan.hpp>
#include <gsl/gsl>

#include <memory>
#include <cstdint>


namespace mirrage {
namespace graphic {

	class Device;

	namespace detail {
		auto clamp_mip_levels(std::uint32_t width, std::uint32_t height, std::uint32_t& mipmaps) -> std::uint32_t;

		class Base_texture {
			public:
				Base_texture(Base_texture&& rhs)noexcept
				    : _image(std::move(rhs._image)), _image_view(std::move(rhs._image_view)) {}

				auto view()const noexcept {return *_image_view;}
				auto image()const noexcept {return _image.image();}

				auto width()const noexcept {return _image.width();}
				auto height()const noexcept {return _image.height();}
				auto depth()const noexcept {return _image.depth();}
				auto layers()const noexcept {return _image.layers();}

			protected:
				// construct as render target
				Base_texture(Device&, Image_type type, Image_dimensions, std::uint32_t mip_levels,
				             vk::Format, vk::ImageUsageFlags, vk::ImageAspectFlags, bool dedicated);

				// construct from loaded data
				Base_texture(Device&, Static_image image, vk::Format);

				// construct as sampled image + load data
				Base_texture(Device&, Image_type type, Image_dimensions, bool generate_mipmaps,
				             vk::Format, gsl::span<const std::uint8_t> data, std::uint32_t owner_qfamily);

				virtual ~Base_texture() = default;

			private:
				Static_image        _image;
				vk::UniqueImageView _image_view;
		};

		extern auto format_from_channels(Device& device, std::uint32_t channels, bool srgb) -> vk::Format;
		extern auto build_mip_views(Device& device, std::uint32_t mip_levels,
		                            vk::Image image,  vk::Format format,
		                            vk::ImageAspectFlags aspects) -> std::vector<vk::UniqueImageView>;
	}

	template<Image_type Type>
	class Texture : public detail::Base_texture {
		public:
			Texture(Device& device, Static_image image, vk::Format format)
			    : Base_texture(device, std::move(image), format) {
			}
			Texture(Device& device, Image_dimensions_t<Type> dim, bool generate_mipmaps,
			        vk::Format format, gsl::span<gsl::byte> data, std::uint32_t owner_qfamily)
			    : Base_texture(device, Type, dim, generate_mipmaps, format, data, owner_qfamily) {
			}
			Texture(Device& device, Image_dimensions_t<Type> dim, bool generate_mipmaps,
			        std::uint32_t channels, bool srgb, gsl::span<const std::uint8_t> data,
			        std::uint32_t owner_qfamily)
			    : Texture(device, Type, dim, generate_mipmaps,
			              detail::format_from_channels(device, channels, srgb), data, owner_qfamily) {
			}

		protected:
			using detail::Base_texture::Base_texture;
	};
	template<Image_type Type>
	class Render_target : public Texture<Type> {
		public:
			Render_target(Device& device, Image_dimensions_t<Type> dim, std::uint32_t mip_levels,
			              vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspects)
			    : Texture<Type>(device, Type, dim, mip_levels, format, usage, aspects, true)
			    , _single_mip_level_views(detail::build_mip_views(device,
			                                                      detail::clamp_mip_levels(dim.width, dim.height, mip_levels),
			                                                      this->image(),
			                                                      format, aspects)) {
			}
			Render_target(Render_target&& rhs)noexcept
			    : Texture<Type>(std::move(rhs))
			    , _single_mip_level_views(std::move(rhs._single_mip_level_views)) {}

			using Texture<Type>::width;
			using Texture<Type>::height;
			using Texture<Type>::view;

			auto view(std::uint32_t level)const noexcept {
				return *_single_mip_level_views.at(level);
			}
			auto width(std::uint32_t level)const noexcept {
				return this->width() / (std::uint32_t(1) << level);
			}
			auto height(std::uint32_t level)const noexcept {
				return this->height() / (std::uint32_t(1) << level);
			}
			auto mip_levels()const noexcept {
				return gsl::narrow<std::uint32_t>(_single_mip_level_views.size());
			}

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

	using Texture_ptr = std::shared_ptr<Texture_2D>;


	class Texture_cache {
		public:
			Texture_cache(Device& device, std::uint32_t owner_qfamily);
	
			auto load(const asset::AID& id) -> Texture_ptr;
	
			void shrink_to_fit();
			
		private:
			Device&                                     _device;
			std::uint32_t                               _owner_qfamily;
			std::unordered_map<asset::AID, Texture_ptr> _textures;
	};

}
}
