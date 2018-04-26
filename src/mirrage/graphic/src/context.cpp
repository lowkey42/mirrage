#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <sf2/sf2.hpp> // has to be first so he sf2_struct define is set

#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/window.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <gsl/gsl>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <unordered_set>


extern "C" {
VkResult vkCreateDebugReportCallbackEXT(VkInstance                                instance,
                                        const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
                                        const VkAllocationCallbacks*              pAllocator,
                                        VkDebugReportCallbackEXT*                 pCallback) {
	auto func = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
	        vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
	if(func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void vkDestroyDebugReportCallbackEXT(VkInstance                   instance,
                                     VkDebugReportCallbackEXT     callback,
                                     const VkAllocationCallbacks* pAllocator) {
	auto func = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
	        vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
	if(func != nullptr) {
		func(instance, callback, pAllocator);
	}
}
}

namespace mirrage::graphic {

	using namespace util::unit_literals;

	namespace {
		void sdl_error_check() {
			const char* err = SDL_GetError();
			if(*err != '\0') {
				std::string errorStr(err);
				SDL_ClearError();
				MIRRAGE_FAIL("SDL: " << errorStr);
			}
		}

		void add_presnet_extensions(std::vector<const char*>& extensions, SDL_Window* window) {
			auto count = static_cast<unsigned int>(0);
			if(!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr)) {
				MIRRAGE_FAIL("Unable to determine present extensions: " << SDL_GetError());
			}

			auto begin = extensions.size();
			extensions.resize(begin + count);

			if(!SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data() + begin)) {
				MIRRAGE_FAIL("Unable to determine present extensions: " << SDL_GetError());
			}
		}

		void add_present_extensions(std::vector<const char*>&                          extensions,
		                            const std::unordered_map<std::string, Window_ptr>& windows) {

			extensions.reserve(extensions.size() + windows.size() * 4);

			for(auto&& [_, window] : windows) {
				(void) _;
				add_presnet_extensions(extensions, window->window_handle());
			}
		}

		void sort_and_unique(std::vector<const char*>& extensions) {
			std::sort(extensions.begin(), extensions.end(), [](auto lhs, auto rhs) {
				return std::strcmp(lhs, rhs) < 0;
			});
			auto new_end = std::unique(extensions.begin(), extensions.end(), [](auto lhs, auto rhs) {
				return std::strcmp(lhs, rhs) == 0;
			});

			extensions.erase(new_end, extensions.end());
		}

		auto check_extensions(const std::vector<const char*>& required,
		                      const std::vector<const char*>& optional) -> std::vector<const char*> {
			auto extensions = std::vector<const char*>();
			extensions.reserve(required.size() + optional.size());

			auto supported_extensions = vk::enumerateInstanceExtensionProperties();

			auto support_confirmed = std::vector<bool>(required.size());

			for(auto& e : supported_extensions) {
				for(auto i = 0u; i < required.size(); i++) {
					if(!strcmp(e.extensionName, required[i])) {
						support_confirmed[i] = true;
						extensions.push_back(required[i]);
						break;
					}
				}

				for(auto i = 0u; i < optional.size(); i++) {
					if(!strcmp(e.extensionName, optional[i])) {
						extensions.push_back(optional[i]);
						break;
					}
				}
			}

			bool all_supported = true;
			for(auto i = 0u; i < support_confirmed.size(); i++) {
				if(!support_confirmed[i]) {
					LOG(plog::warning) << "Unsupported extension \"" << required[i] << "\"!";
					all_supported = false;
				}
			}

			if(!all_supported) {
				MIRRAGE_FAIL("At least one required extension is not supported (see log for details)!");
			}

			return extensions;
		}

		auto check_layers(const std::vector<const char*>& requested) -> std::vector<const char*> {
			auto validation_layers = std::vector<const char*>();
			validation_layers.reserve(requested.size());

			auto supported_layers = vk::enumerateInstanceLayerProperties();
			for(auto& l : supported_layers) {
				bool layer_requested = false;
				for(auto& req_layer : requested) {
					if(0 == strcmp(l.layerName, req_layer)) {
						validation_layers.push_back(req_layer);
						layer_requested = true;
						break;
					}
				}

				if(!layer_requested) {
					LOG(plog::debug) << "Additional validation layer is available, that hasn't been "
					                    "requested: "
					                 << l.layerName;
				}
			}

			if(validation_layers.size() != requested.size()) {
				IF_LOG(plog::error) {
					auto msg = std::stringstream{};
					msg << "Some requested validation layers are not supported: \n";
					for(auto& l : validation_layers) {
						msg << "  - " << l << "\n";
					}

					LOG(plog::error) << msg.str();
				}
			}

			return validation_layers;
		}

		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT      flags,
		                                             VkDebugReportObjectTypeEXT objType,
		                                             uint64_t                   obj,
		                                             size_t                     location,
		                                             int32_t                    code,
		                                             const char*                layerPrefix,
		                                             const char*                msg,
		                                             void*                      userData) {

			// silences: DescriptorSet 0x3f previously bound as set #1 is incompatible with set
			//             0x1cc99c0 newly bound as set #1 so set #2 and any subsequent sets were
			//             disturbed by newly bound pipelineLayout (0x4f)
			if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT)
				return VK_FALSE;


			if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
				LOG(plog::error) << "[VK | " << layerPrefix << "] " << msg;
			} else if(flags & VK_DEBUG_REPORT_WARNING_BIT_EXT
			          || flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
				LOG(plog::warning) << "[VK | " << layerPrefix << "] " << msg;
			} else {
				LOG(plog::info) << "[VK | " << layerPrefix << "] " << msg;
			}

			return VK_FALSE;
		}
	} // namespace


	Context::Context(const std::string&    appName,
	                 uint32_t              appVersion,
	                 const std::string&    engineName,
	                 uint32_t              engineVersion,
	                 bool                  debug,
	                 asset::Asset_manager& assets)
	  : _assets(assets), _name(appName) {

		auto maybe_settings = assets.load_maybe<Graphics_settings>("cfg:graphics"_aid);
		if(maybe_settings.is_nothing()) {
			if(!settings(default_settings())) {
				MIRRAGE_FAIL("Invalid graphics settings");
			}
		} else {
			_settings = maybe_settings.get_or_throw();
		}

		if(!settings(*_settings)) { //< apply actual size/settings
			MIRRAGE_FAIL("Couldn't apply graphics settings");
		}

		sdl_error_check();

		for(auto&& [title, win] : _settings->windows) {
			_windows.emplace(title,
			                 std::make_unique<Window>(title,
			                                          app_name() + " | " + title,
			                                          win.display,
			                                          win.width,
			                                          win.height,
			                                          win.fullscreen));
		}

		auto instanceCreateInfo = vk::InstanceCreateInfo{};
		auto appInfo            = vk::ApplicationInfo{
                appName.c_str(), appVersion, engineName.c_str(), engineVersion, VK_API_VERSION_1_0};

		auto required_extensions = std::vector<const char*>{VK_KHR_SURFACE_EXTENSION_NAME};
		add_present_extensions(required_extensions, _windows);
		auto optional_extensions = std::vector<const char*>{};


		if(debug) {
			_enabled_layers = check_layers({"VK_LAYER_LUNARG_standard_validation"});

			required_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		sort_and_unique(required_extensions);
		sort_and_unique(optional_extensions);
		auto extensions = check_extensions(required_extensions, optional_extensions);

		IF_LOG(plog::info) {
			auto msg = std::stringstream{};
			msg << "Initializing vulkan...\n";
			msg << "Enabled extensions:\n";
			for(auto e : extensions) {
				msg << "  - " << e << "\n";
			}
			msg << "Enabled validation layers:\n";
			for(auto l : _enabled_layers) {
				msg << "  - " << l << "\n";
			}

			LOG(plog::info) << msg.str();
		}

		instanceCreateInfo.setPApplicationInfo(&appInfo);
		instanceCreateInfo.setEnabledExtensionCount(gsl::narrow<uint32_t>(extensions.size()));
		instanceCreateInfo.setPpEnabledExtensionNames(extensions.data());
		instanceCreateInfo.setEnabledLayerCount(gsl::narrow<uint32_t>(_enabled_layers.size()));
		instanceCreateInfo.setPpEnabledLayerNames(_enabled_layers.data());
		_instance = vk::createInstanceUnique(instanceCreateInfo);

		if(debug) {
			_debug_callback = _instance->createDebugReportCallbackEXTUnique(
			        {/*	vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eInformation | */
			         vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::ePerformanceWarning
			                 | vk::DebugReportFlagBitsEXT::eWarning,
			         debugCallback});
		}

		for(auto&& [_, window] : _windows) {
			(void) _;
			window->create_surface(*this);
		}
	}
	Context::~Context() = default;

	bool Context::settings(Graphics_settings new_settings) {
		_assets.save<Graphics_settings>("cfg:graphics"_aid, new_settings);
		_settings = _assets.load<Graphics_settings>("cfg:graphics"_aid);

		return true;
	}

	auto Context::find_window(std::string name) -> util::maybe<Window&> {
		auto iter = _windows.find(name);
		return iter == _windows.end() ? util::nothing : util::justPtr(&*iter->second);
	}
	auto Context::_find_window_settings(const std::string& name, int width, int height) -> Window_settings {
		auto& cfg = settings();

		auto win_cfg = std::find_if(
		        std::begin(cfg.windows), std::end(cfg.windows), [&](auto& w) { return w.first == name; });

		if(win_cfg == std::end(cfg.windows)) { // no config create new
			auto new_settings = cfg;
			auto win_settings = default_window_settings(0);
			if(width > 0)
				win_settings.width = width;
			if(height > 0)
				win_settings.height = height;
			new_settings.windows[name] = win_settings;

			settings(new_settings);

			return win_settings;
		}

		return win_cfg->second;
	}

	namespace {
		bool supports_present(vk::PhysicalDevice& gpu, const std::vector<Window*>& can_present_to) {
			auto supported_extensions = gpu.enumerateDeviceExtensionProperties();
			auto sc_ext = std::find_if(supported_extensions.begin(), supported_extensions.end(), [](auto& e) {
				return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
			});

			if(sc_ext == supported_extensions.end()) {
				return false;
			}

			for(auto window : can_present_to) {
				auto formats       = gpu.getSurfaceFormatsKHR(window->surface());
				auto present_modes = gpu.getSurfacePresentModesKHR(window->surface());

				if(formats.empty() || present_modes.empty())
					return false;
			}

			return true;
		}

		auto find_graphics_queue(vk::PhysicalDevice& gpu, const std::vector<Window*>& can_present_to)
		        -> util::maybe<std::uint32_t> {
			auto i = 0;

			for(auto& queue_family : gpu.getQueueFamilyProperties()) {
				auto can_present =
				        can_present_to.empty()
				        || std::all_of(can_present_to.begin(), can_present_to.end(), [&](auto window) {
					           return gpu.getSurfaceSupportKHR(i, window->surface());
				           });

				if(queue_family.queueCount > 0 && can_present
				   && queue_family.timestampValidBits
				              >= 32 // only accept queues with >=32bit timer support, for now
				   && (queue_family.queueFlags & vk::QueueFlagBits::eGraphics)) {
					return i;
				}

				i++;
			}

			return util::nothing;
		}

		auto find_transfer_queue(vk::PhysicalDevice& gpu) -> std::uint32_t {
			auto families = gpu.getQueueFamilyProperties();

			auto i = 0;

			// check for transfer-only queue
			for(auto& queue_family : families) {
				if(queue_family.queueCount > 0 && (queue_family.queueFlags & vk::QueueFlagBits::eTransfer)
				   && !(queue_family.queueFlags & vk::QueueFlagBits::eGraphics)) {
					return i;
				}

				i++;
			}

			i = 0;
			for(auto& queue_family : families) {
				if(queue_family.queueCount > 0
				   && ((queue_family.queueFlags & vk::QueueFlagBits::eTransfer)
				       || (queue_family.queueFlags & vk::QueueFlagBits::eGraphics)
				       || (queue_family.queueFlags & vk::QueueFlagBits::eCompute))) {
					return i;
				}

				i++;
			}

			MIRRAGE_FAIL("No queue found, that supports transfer operations!");
		}

		auto find_surface_format(vk::PhysicalDevice& gpu,
		                         Window&             window,
		                         vk::Format          target_format,
		                         vk::ColorSpaceKHR   target_space) -> vk::SurfaceFormatKHR {

			auto formats = gpu.getSurfaceFormatsKHR(window.surface());
			if(formats.size() == 1 && formats.front().format == vk::Format::eUndefined) {
				auto surface_format       = vk::SurfaceFormatKHR{};
				surface_format.format     = target_format;
				surface_format.colorSpace = target_space;
				return surface_format;
			}

			auto opt_found = std::find_if(formats.begin(), formats.end(), [&](auto& f) {
				return f.format == target_format && f.colorSpace == target_space;
			});
			if(opt_found != formats.end()) {
				auto surface_format       = vk::SurfaceFormatKHR{};
				surface_format.format     = target_format;
				surface_format.colorSpace = target_space;
				return surface_format;
			}

			opt_found = std::find_if(
			        formats.begin(), formats.end(), [&](auto& f) { return f.format == target_format; });
			if(opt_found != formats.end()) {
				return *opt_found;
			}

			if(!formats.empty()) {
				LOG(plog::warning) << "Requested format is not supported by the device, fallback to first "
				                      "reported format";
				return formats.front();
			} else {
				MIRRAGE_FAIL("The device doesn't support any surface formats!");
			}
		}

		auto create_swapchain_create_info(vk::PhysicalDevice          gpu,
		                                  bool                        srgb,
		                                  const std::vector<Window*>& can_present_to) {
			auto swapchains = Swapchain_create_infos();

			for(auto window : can_present_to) {
				auto capabilities = gpu.getSurfaceCapabilitiesKHR(window->surface());


				auto sc_info = vk::SwapchainCreateInfoKHR{};
				sc_info.setSurface(window->surface());
				sc_info.setClipped(true);
				sc_info.setPreTransform(capabilities.currentTransform);
				sc_info.setImageSharingMode(vk::SharingMode::eExclusive);
				sc_info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment
				                      | vk::ImageUsageFlagBits::eTransferDst);
				sc_info.setImageArrayLayers(1);
				sc_info.setMinImageCount(std::max(3u, capabilities.minImageCount));
				if(capabilities.maxImageCount > 0 && capabilities.maxImageCount < sc_info.minImageCount) {
					sc_info.setMinImageCount(capabilities.maxImageCount);
				}

				auto present_modes = gpu.getSurfacePresentModesKHR(window->surface());
				auto mailbox_supported =
				        std::find(present_modes.begin(), present_modes.end(), vk::PresentModeKHR::eMailbox)
				        != present_modes.end();
				if(mailbox_supported) {
					sc_info.setPresentMode(vk::PresentModeKHR::eMailbox);
				} else {
					sc_info.setPresentMode(vk::PresentModeKHR::eFifo);
				}

				if(capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
					capabilities.currentExtent.width =
					        std::min(std::max(gsl::narrow<std::uint32_t>(window->width()),
					                          capabilities.minImageExtent.width),
					                 capabilities.maxImageExtent.width);

					capabilities.currentExtent.height =
					        std::min(std::max(gsl::narrow<std::uint32_t>(window->height()),
					                          capabilities.minImageExtent.height),
					                 capabilities.maxImageExtent.height);
				}

				sc_info.setImageExtent(capabilities.currentExtent);

				auto format =
				        find_surface_format(gpu,
				                            *window,
				                            srgb ? vk::Format::eB8G8R8A8Srgb : vk::Format::eB8G8R8A8Unorm,
				                            vk::ColorSpaceKHR::eSrgbNonlinear);

				sc_info.setImageFormat(format.format);
				sc_info.setImageColorSpace(format.colorSpace);

				swapchains.emplace(window->name(), std::make_tuple(window, sc_info));
			}

			return swapchains;
		}
	} // namespace
	auto Context::instantiate_device(Device_selector             selector,
	                                 Device_factory              factory,
	                                 const std::vector<Window*>& can_present_to,
	                                 bool                        srgb) -> Device_ptr {
		auto top_score = std::numeric_limits<int>::min();
		auto top_gpu   = vk::PhysicalDevice{};

		for(auto& gpu : _instance->enumeratePhysicalDevices()) {
			if(!can_present_to.empty() && !supports_present(gpu, can_present_to)) {
				continue;
			}

			auto graphics_queue = find_graphics_queue(gpu, can_present_to);

			auto score = selector(gpu, graphics_queue);

			auto gpu_name = std::string(gpu.getProperties().deviceName);
			LOG(plog::info) << "Detected GPU: " << gpu_name;

			if(!_settings->gpu_preference.empty() && _settings->gpu_preference == gpu_name) {
				score = std::numeric_limits<int>::max();
			}

			if(score > top_score) {
				top_score = score;
				top_gpu   = gpu;
			}
		}

		if(!top_gpu) {
			MIRRAGE_FAIL("Couldn't find a GPU that supports vulkan and all required features.");
		}

		LOG(plog::info) << "Selected GPU: " << top_gpu.getProperties().deviceName;

		auto cfg = vk::DeviceCreateInfo{};
		cfg.setEnabledLayerCount(gsl::narrow<uint32_t>(_enabled_layers.size()));
		cfg.setPpEnabledLayerNames(_enabled_layers.data());

		auto extensions           = std::vector<const char*>();
		auto supported_extensions = top_gpu.enumerateDeviceExtensionProperties();
		auto extension_supported  = [&](const char* e) {
            return supported_extensions.end()
                   != std::find_if(supported_extensions.begin(), supported_extensions.end(), [&](auto& se) {
                          return !strcmp(se.extensionName, e);
                      });
		};

		auto dedicated_alloc_supported = false;

#ifdef VK_NV_dedicated_allocation
		if(extension_supported(VK_NV_DEDICATED_ALLOCATION_EXTENSION_NAME)) {
			extensions.emplace_back(VK_NV_DEDICATED_ALLOCATION_EXTENSION_NAME);
			dedicated_alloc_supported = true;
		}
#endif

		if(!can_present_to.empty()) {
			extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		cfg.setEnabledExtensionCount(extensions.size());
		cfg.setPpEnabledExtensionNames(extensions.data());

		auto create_info = factory(top_gpu, find_graphics_queue(top_gpu, can_present_to));

		cfg.setPEnabledFeatures(&create_info.features);

		// familyId => (count, priorities)
		auto queue_families =
		        std::unordered_map<std::uint32_t, std::tuple<std::uint32_t, std::vector<float>>>();

		auto available_families = top_gpu.getQueueFamilyProperties();

		auto queue_mapping = Queue_family_mapping();

		auto transfer_queue_tag = "_transfer"_strid;
		create_info.queue_families.emplace(transfer_queue_tag,
		                                   Queue_create_info{find_transfer_queue(top_gpu), 0.2f});

		auto draw_queue_tag = util::maybe<util::Str_id>(util::nothing);

		for(auto& qf : create_info.queue_families) {
			auto tag      = qf.first;
			auto family   = qf.second.family_id;
			auto priority = qf.second.priority;

			auto& entry = queue_families[family];

			if(available_families[family].queueFlags & vk::QueueFlagBits::eGraphics) {
				draw_queue_tag = qf.first;
			} else if(draw_queue_tag.is_nothing()
			          && (available_families[family].queueFlags & vk::QueueFlagBits::eCompute))
				draw_queue_tag = qf.first;

			if(available_families[family].queueCount > 0) {
				available_families[family].queueCount--;
				std::get<0>(entry) += 1;
				std::get<1>(entry).emplace_back(priority);
			} else {
				LOG(plog::warning) << "More queues requested than are availbalbe from family " << family
				                   << ". Collapsed with previous queue!";
			}

			queue_mapping.emplace(tag, std::make_tuple(family, std::get<1>(entry).size() - 1));
		}

		auto used_queues = std::vector<vk::DeviceQueueCreateInfo>{};
		used_queues.reserve(queue_families.size());
		for(auto& qf : queue_families) {
			MIRRAGE_INVARIANT(std::get<1>(qf.second).size() == std::get<0>(qf.second), "Size mismatch");

			used_queues.emplace_back(vk::DeviceQueueCreateFlags(),
			                         qf.first,
			                         std::get<0>(qf.second),
			                         std::get<1>(qf.second).data());
		}

		cfg.setQueueCreateInfoCount(gsl::narrow<std::uint32_t>(used_queues.size()));
		cfg.setPQueueCreateInfos(used_queues.data());

		auto swapchains = create_swapchain_create_info(top_gpu, srgb, can_present_to);

		return std::make_unique<Device>(
		        *this,
		        _assets,
		        top_gpu.createDeviceUnique(cfg),
		        top_gpu,
		        transfer_queue_tag,
		        draw_queue_tag.get_or_throw("No draw or compute queue! That doesn't seem right."),
		        std::move(queue_mapping),
		        std::move(swapchains),
		        dedicated_alloc_supported);
	}
} // namespace mirrage::graphic
