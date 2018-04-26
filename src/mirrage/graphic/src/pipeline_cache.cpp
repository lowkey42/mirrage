#include <mirrage/graphic/pipeline_cache.hpp>

#include <mirrage/graphic/device.hpp>

#include <gsl/gsl>


namespace mirrage::graphic {

	constexpr auto max_pipeline_cache_count = 3;

	namespace {
		auto main_pipeline_cache_key(const Device& device) {
			auto properties = device.physical_device_properties();

			return std::to_string(properties.vendorID) + "_" + std::to_string(properties.deviceID);
		}

		auto find_pipeline_cache_aid(asset::Asset_manager& assets, const std::string& key)
		        -> util::maybe<asset::AID> {

			auto caches = assets.list("pl_cache"_strid);
			for(auto& cache : caches) {
				if(util::ends_with(cache.name(), key)) {
					return cache;
				}
			}

			return util::nothing;
		}

		auto discard_outdated_pipeline_caches(asset::Asset_manager& assets) {
			auto caches = assets.list("pl_cache"_strid);

			if(caches.size() >= max_pipeline_cache_count) {
				LOG(plog::debug) << "More than " << max_pipeline_cache_count
				                 << " pipeline cahces. Deleting oldest caches.";

				std::sort(std::begin(caches), std::end(caches), [&](auto& lhs, auto& rhs) {
					return assets.last_modified(lhs).get_or(-1) > assets.last_modified(rhs).get_or(-1);
				});

				std::for_each(
				        std::begin(caches) + max_pipeline_cache_count - 1, std::end(caches), [&](auto& aid) {
					        if(!assets.try_delete(aid)) {
						        LOG(plog::warning)
						                << "Unable to delete outdated pipeline cache: " + aid.str();
					        }
				        });
			}
		}

	} // namespace

	auto load_main_pipeline_cache(const Device& device, asset::Asset_manager& assets)
	        -> asset::Ptr<Pipeline_cache> {

		auto key = main_pipeline_cache_key(device);

		auto aid = find_pipeline_cache_aid(assets, key).get_or([&] {
			LOG(plog::debug) << "No pipeline cache found for device, creating new one: dev_" << key;
			discard_outdated_pipeline_caches(assets);
			return asset::AID{"pl_cache"_strid, "dev_" + key};
		});

		return assets.load_maybe<Pipeline_cache>(aid).get_or([&] {
			return asset::make_ready_asset(
			        aid, device.vk_device()->createPipelineCacheUnique(vk::PipelineCacheCreateInfo{}));
		});
	}

} // namespace mirrage::graphic

namespace mirrage::asset {

	auto Loader<graphic::Pipeline_cache>::load(istream in) -> graphic::Pipeline_cache {

		auto data = in.bytes();
		auto create_info =
		        vk::PipelineCacheCreateInfo{vk::PipelineCacheCreateFlags{}, data.size(), data.data()};

		return graphic::Pipeline_cache(_device.createPipelineCacheUnique(create_info));
	}

	void Loader<graphic::Pipeline_cache>::save(ostream out, const graphic::Pipeline_cache& cache) {
		auto data = _device.getPipelineCacheData(*cache);
		out.write(reinterpret_cast<const char*>(data.data()), gsl::narrow<std::streamsize>(data.size()));
	}

} // namespace mirrage::asset
