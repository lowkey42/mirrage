#pragma once

#include <mirrage/asset/stream.hpp>
#include <mirrage/graphic/vk_wrapper.hpp>
#include <mirrage/utils/maybe.hpp>

#include <vulkan/vulkan.hpp>


namespace mirrage::graphic {

	// https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
	struct Ktx_header {
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t depth;
		std::uint32_t layers;
		bool          cubemap;
		std::uint32_t mip_levels;
		std::uint32_t size;
		vk::Format    format;
		Image_type    type;
	};

	// reads the KTX header and advances the read position to the beginning of the actual data
	extern auto parse_header(asset::istream&, const std::string& filename) -> util::maybe<Ktx_header>;

} // namespace mirrage::graphic
