#pragma once

#include <mirrage/asset/asset_manager.hpp>

#include <vulkan/vulkan.hpp>


namespace mirrage::graphic {

	class Device;


	using Pipeline_cache = vk::UniquePipelineCache;

	extern auto load_main_pipeline_cache(const Device&, asset::Asset_manager&)
	        -> asset::Loading<Pipeline_cache>;

} // namespace mirrage::graphic

namespace mirrage::asset {

	template <>
	struct Loader<graphic::Pipeline_cache> {
	  public:
		Loader(vk::Device device) : _device(device) {}

		auto load(istream in) -> std::shared_ptr<graphic::Pipeline_cache>;
		void save(ostream out, const graphic::Pipeline_cache& asset);

	  private:
		vk::Device _device;
	};

} // namespace mirrage::asset
