#pragma once

#include <mirrage/graphic/settings.hpp>

#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <vulkan/vulkan.hpp>
#include <range/v3/view.hpp>

#include <vector>
#include <memory>
#include <string>


namespace lux {
	namespace asset{
		class Asset_manager;
		class AID;
	}

namespace graphic {

	struct Queue_create_info {
		std::uint32_t family_id;
		float priority=1.f;
	};

	struct Device_create_info {
		std::unordered_map<util::Str_id, Queue_create_info> queue_families;
		vk::PhysicalDeviceFeatures features;
	};

	using Device_selector = std::function<int(vk::PhysicalDevice,
	                                          util::maybe<std::uint32_t>)>;
	using Device_factory  = std::function<Device_create_info(
	    vk::PhysicalDevice,
	    util::maybe<std::uint32_t> graphic_queue_family)>;


	class Context : public util::Registration<Context, Device>,
	                public util::Registration<Context, Window> {
		public:
			Context(const std::string& appName, uint32_t appVersion,
			        const std::string& engineName, uint32_t engineVersion,
			        bool debug, asset::Asset_manager& assets);
			~Context();

			auto settings()const noexcept -> const Graphics_settings& {return *_settings;}
			bool settings(Graphics_settings);

			auto app_name()const noexcept -> const std::string& {return _name;}

			auto instantiate_device(Device_selector, Device_factory,
			                        const std::vector<Window*>& can_present_to={},
			                        bool srgb=false) -> Device_ptr;

			auto create_window(std::string name,
			                   int width=-1, int height=-1) -> Window_ptr;

			auto list_windows() -> auto& {
				return util::Registration<Context, Window>::children();
			}

			auto instance() -> auto& {return *_instance;}

			auto asset_manager() -> auto& {return _assets;}

		private:
			asset::Asset_manager& _assets;
			std::string _name;
			std::shared_ptr<const Graphics_settings> _settings;
			std::vector<const char*> _enabled_layers;

			vk::UniqueInstance _instance;
			vk::UniqueDebugReportCallbackEXT _debug_callback;

			auto _find_window_settings(const std::string& name,
			                           int width, int height) -> Window_settings;
	};

}
}
