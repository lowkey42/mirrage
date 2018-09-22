#include "ktx_parser.hpp"

#include "vk_format.h"

#include <mirrage/utils/log.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	// heavily based on load_ktx from GLI

	namespace {
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
	} // namespace

	auto parse_header(asset::istream& in, const std::string& filename) -> Ktx_header
	{
		auto type_tag_data = std::array<unsigned char, sizeof(type_tag)>();

		in.read(reinterpret_cast<char*>(type_tag_data.data()), type_tag_data.size());
		if(memcmp(type_tag_data.data(), type_tag, type_tag_data.size()) != 0) {
			MIRRAGE_FAIL("Invalid KTX file: " << filename);
		}

		auto header = ktx_header10();
		in.read(reinterpret_cast<char*>(&header), sizeof(ktx_header10));

		// skip to pixel data
		in.ignore(header.BytesOfKeyValueData);

		auto bytes_left =
		        in.length() - type_tag_data.size() - sizeof(ktx_header10) - header.BytesOfKeyValueData;

		MIRRAGE_INVARIANT(bytes_left > 0, "Can't load empty image \"" << filename << "\" ");


		auto our_header       = Ktx_header();
		our_header.width      = header.PixelWidth;
		our_header.height     = header.PixelHeight;
		our_header.depth      = header.PixelDepth;
		our_header.layers     = header.NumberOfArrayElements;
		our_header.cubemap    = header.NumberOfFaces == 6;
		our_header.mip_levels = header.NumberOfMipmapLevels;
		our_header.size       = gsl::narrow<std::uint32_t>(bytes_left);

		our_header.format = static_cast<vk::Format>(
		        vkGetFormatFromOpenGLInternalFormat(static_cast<GLenum>(header.GLInternalFormat)));

		if(our_header.cubemap) {
			our_header.type = our_header.layers == 0 ? Image_type::cubemap : Image_type::array_cubemap;
		} else if(our_header.depth > 0) { // 3D
			our_header.type = our_header.layers == 0 ? Image_type::single_3d : Image_type::array_3d;
		} else if(our_header.height > 0) { // 2D
			our_header.type = our_header.layers == 0 ? Image_type::single_2d : Image_type::array_2d;
		} else if(our_header.width > 0) { // 1D
			our_header.type = our_header.layers == 0 ? Image_type::single_1d : Image_type::array_1d;
		} else {
			MIRRAGE_FAIL("Zero dimensional image (width, height and depth are all zero): " << filename);
		}

		return our_header;
	}
} // namespace mirrage::graphic
