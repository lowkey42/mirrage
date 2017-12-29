#pragma once

#include <mirrage/graphic/settings.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/units.hpp>

#include <vulkan/vulkan.hpp>

#include <memory>
#include <string>
#include <vector>


namespace mirrage {
	namespace asset {
		class Asset_manager;
		class AID;
	} // namespace asset

	namespace graphic {

		struct Queue_create_info {
			std::uint32_t family_id;
			float         priority = 1.f;
		};

		struct Device_create_info {
			std::unordered_map<util::Str_id, Queue_create_info> queue_families;
			vk::PhysicalDeviceFeatures                          features;
		};

		using Device_selector = std::function<int(vk::PhysicalDevice, util::maybe<std::uint32_t>)>;
		using Device_factory  = std::function<Device_create_info(
                vk::PhysicalDevice, util::maybe<std::uint32_t> graphic_queue_family)>;


		class Context : public util::Registration<Context, Device> {
		  public:
			Context(const std::string&    appName,
			        uint32_t              appVersion,
			        const std::string&    engineName,
			        uint32_t              engineVersion,
			        bool                  debug,
			        asset::Asset_manager& assets);
			~Context();

			auto settings() const noexcept -> const Graphics_settings& { return *_settings; }
			bool settings(Graphics_settings);

			auto app_name() const noexcept -> const std::string& { return _name; }

			auto instantiate_device(Device_selector,
			                        Device_factory,
			                        const std::vector<Window*>& can_present_to = {},
			                        bool                        srgb = false) -> Device_ptr;

			auto find_window(std::string name) -> util::maybe<Window&>;

			auto list_windows() -> auto& { return _windows; }

			auto instance() -> auto& { return *_instance; }

			auto asset_manager() -> auto& { return _assets; }

		  private:
			asset::Asset_manager&                       _assets;
			std::string                                 _name;
			std::shared_ptr<const Graphics_settings>    _settings;
			std::vector<const char*>                    _enabled_layers;
			std::unordered_map<std::string, Window_ptr> _windows;

			vk::UniqueInstance               _instance;
			vk::UniqueDebugReportCallbackEXT _debug_callback;

			auto _find_window_settings(const std::string& name, int width, int height)
			        -> Window_settings;
		};
	} // namespace graphic
} // namespace mirrage
