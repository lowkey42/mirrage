#pragma once

#include <mirrage/renderer/billboard.hpp>
#include <mirrage/renderer/decal.hpp>
#include <mirrage/renderer/object_router.hpp>
#include <mirrage/renderer/particle_system.hpp>

#include <mirrage/ecs/entity_handle.hpp>
#include <mirrage/graphic/context.hpp>
#include <mirrage/graphic/profiler.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/ranges.hpp>
#include <mirrage/utils/reflection.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/gtx/quaternion.hpp>
#include <glm/vec3.hpp>
#include <vulkan/vulkan.hpp>

#include <functional>
#include <type_traits>
#include <variant>


namespace mirrage {
	class Engine;
} // namespace mirrage

namespace mirrage::ecs {
	class Entity_manager;
	namespace components {
		struct Transform_comp;
	}
} // namespace mirrage::ecs

namespace mirrage::graphic {
	struct Device_create_info;
} // namespace mirrage::graphic

namespace mirrage::renderer {
	class Deferred_renderer;
	class Model;
	class Directional_light_comp;
	class Point_light_comp;
	struct Camera_state;
	struct Sub_mesh;
} // namespace mirrage::renderer


namespace mirrage::renderer {
	using Command_buffer_source = std::function<vk::CommandBuffer()>;

	struct Debug_geometry {
		glm::vec3 start;
		glm::vec3 end;
		util::Rgb color;

		Debug_geometry() = default;
		Debug_geometry(const glm::vec3& start, const glm::vec3& end, const util::Rgb& color)
		  : start(start), end(end), color(color)
		{
		}
	};

	class Frame_data {
	  public:
		vk::CommandBuffer main_command_buffer;
		vk::DescriptorSet global_uniform_set;
		std::size_t       swapchain_image;
		Camera_state*     camera;
		Culling_mask      camera_culling_mask;

		std::vector<Debug_geometry> debug_geometry_queue;
	};

	class Render_pass {
	  public:
		Render_pass(Deferred_renderer& r) : _renderer(r) {}
		virtual ~Render_pass() = default;

		virtual void update(util::Time dt) {}

		virtual void process_camera(Camera_state&) {} //< allows passes to modify the current camera

		// template<typename... P> void pre_draw (Frame_data&, Object_router<P...>&);
		//                         void pre_draw (Frame_data&);
		// template<typename... P> void post_draw(Frame_data&, Object_router<P...>&);
		//                         void post_draw(Frame_data&);
		// template<typename... P> void on_draw  (Frame_data&, Object_router<P...>&);
		//                         void on_draw  (Frame_data&);
		//                         void handle_obj(Frame_data&, Culling_mask, ...);
		// template<typename... P> void handle_obj(Frame_data&, Object_router<P...>&, Culling_mask, ...);

		virtual auto name() const noexcept -> const char* = 0;

		/// API to allow render passes to save some of their state (e.g. loaded textures/data)
		///   across pipeline/renderer recreation
		virtual auto extract_persistent_state() -> std::shared_ptr<void> { return {}; }

	  protected:
		Deferred_renderer& _renderer;

		struct Raii_marker {
			graphic::Queue_debug_label   label;
			graphic::Profiler::Push_raii profiler;
		};

		auto _mark_subpass(Frame_data&) -> Raii_marker;
	};

	using Render_pass_id = util::type_uid_t;

	class Render_pass_factory {
	  public:
		Render_pass_factory();
		virtual ~Render_pass_factory() = default;

		virtual auto id() const noexcept -> Render_pass_id = 0;

		virtual auto create_pass(Deferred_renderer&,
		                         std::shared_ptr<void> last_state,
		                         util::maybe<ecs::Entity_manager&>,
		                         Engine&,
		                         bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass> = 0;

		virtual auto requires_gbuffer() const noexcept -> bool { return true; }

		virtual auto rank_device(vk::PhysicalDevice,
		                         util::maybe<std::uint32_t> graphics_queue,
		                         int                        current_score) -> int
		{
			(void) graphics_queue;
			return current_score;
		}

		virtual void configure_device(vk::PhysicalDevice,
		                              util::maybe<std::uint32_t> graphics_queue,
		                              graphic::Device_create_info&)
		{
			(void) graphics_queue;
		}
	};

	template <class T>
	auto render_pass_id_of()
	{
		if constexpr(std::is_base_of<Render_pass_factory, T>::value)
			return util::type_uid_of<T>();
		else {
			static_assert(std::is_base_of<Render_pass_factory, typename T::Factory>::value,
			              "T is not a renderpass, nor its factory.");
			return util::type_uid_of<typename T::Factory>();
		}
	}

} // namespace mirrage::renderer
