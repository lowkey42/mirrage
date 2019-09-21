#pragma once

#include <mirrage/graphic/settings.hpp>

#include <mirrage/asset/asset_manager.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/registration.hpp>
#include <mirrage/utils/str_id.hpp>
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
			                        bool                        srgb           = false) -> Device_ptr;

			auto find_window(std::string name) -> util::maybe<Window&>;

			auto list_windows() -> auto& { return _windows; }

			auto instance() -> auto& { return *_instance; }

			auto asset_manager() -> auto& { return _assets; }

			void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer             commandBuffer,
			                                  const VkDebugUtilsLabelEXT* pLabelInfo);
			void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer);

		  private:
			bool                          _debug;
			asset::Asset_manager&         _assets;
			std::string                   _name;
			asset::Ptr<Graphics_settings> _settings;
			std::vector<const char*>      _enabled_layers;

			vk::UniqueInstance               _instance;
			vk::UniqueDebugUtilsMessengerEXT _debug_callback;

			std::unordered_map<std::string, Window_ptr> _windows;

			PFN_vkCmdBeginDebugUtilsLabelEXT _vkCmdBeginDebugUtilsLabelEXT = nullptr;
			PFN_vkCmdEndDebugUtilsLabelEXT   _vkCmdEndDebugUtilsLabelEXT   = nullptr;

			auto _find_window_settings(const std::string& name, int width, int height) -> Window_settings;
		};

		class Queue_debug_label {
		  public:
			Queue_debug_label(Context&, vk::CommandBuffer, const char* name);
			~Queue_debug_label();

		  private:
			Context*          _ctx;
			vk::CommandBuffer _cmds;
		};

	} // namespace graphic
} // namespace mirrage
