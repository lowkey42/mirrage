#pragma once

#include <mirrage/utils/func_traits.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/ranges.hpp>
#include <mirrage/utils/reflection.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <array>
#include <tuple>
#include <type_traits>

namespace mirrage::renderer {

	using Culling_mask = std::uint32_t;

	class Frame_data;
	class Render_pass;

	namespace detail {
#define MIRRAGE_CHECK_PASS_METHOD_STAMP(NAME)                                                    \
	template <class Pass, class... Args>                                                         \
	struct has_##NAME {                                                                          \
	  private:                                                                                   \
		template <typename C>                                                                    \
		static std::true_type test(decltype(std::declval<C>()->NAME(std::declval<Args>()...))*); \
		template <typename C>                                                                    \
		static std::false_type test(...);                                                        \
                                                                                                 \
		using type = decltype(test<Pass>(nullptr));                                              \
                                                                                                 \
	  public:                                                                                    \
		static constexpr bool value = type::value;                                               \
	}

		MIRRAGE_CHECK_PASS_METHOD_STAMP(handle_obj);
		MIRRAGE_CHECK_PASS_METHOD_STAMP(pre_draw);
		MIRRAGE_CHECK_PASS_METHOD_STAMP(post_draw);
		MIRRAGE_CHECK_PASS_METHOD_STAMP(on_draw);

#undef MIRRAGE_CHECK_PASS_METHOD_STAMP

		struct Has_handle_obj_helper_base {
			void handle_obj();
		};
		template <typename T>
		struct Has_handle_obj_helper : T, Has_handle_obj_helper_base {
		};

		template <typename T>
		using has_no_foo_template = decltype(std::declval<Has_handle_obj_helper<T>>().handle_obj());

		template <typename T>
		constexpr auto has_any_handle_obj = !util::is_detected_v<has_no_foo_template, T>;

		using Frustum_planes = std::array<glm::vec4, 6>;
		struct Culling_viewer {
			Frustum_planes planes;
			bool           is_camera;

			Culling_viewer() = default;
			Culling_viewer(Frustum_planes planes, bool is_camera = true)
			  : planes(planes), is_camera(is_camera)
			{
			}

			auto visible(glm::vec4 p, float radius)
			{
				if(radius < 0)
					return true;

				auto result = true;
				for(auto& plane : planes)
					result = result & (glm::dot(plane, p) > -radius);

				return result;
			}
		};

	} // namespace detail

	class Object_router_base {
	  public:
		virtual ~Object_router_base();

		virtual auto active() const -> bool = 0;
		virtual void pre_draw(Frame_data& fd)
		{
			_frame_data = &fd;
			_frustums.clear();
		}

		virtual void post_draw(Frame_data& fd) { _frame_data = nullptr; }

		virtual void on_draw(Frame_data& fd) = 0;

		/// add viewer (camera/light/...), must be called during pre_draw
		auto add_viewer(const glm::mat4& view_proj, bool is_camera) -> Culling_mask;

	  protected:
		Frame_data*                         _frame_data = nullptr;
		std::vector<detail::Culling_viewer> _frustums;

		auto _is_visible(glm::vec3 position, float radius, bool camera_only) -> Culling_mask
		{
			const auto p    = glm::vec4(position, 1.f);
			auto       mask = Culling_mask(0);

			for(auto [i, f] : util::with_index(_frustums)) {
				if((!camera_only || f.is_camera) && f.visible(p, radius)) {
					mask |= Culling_mask(1) << i;
				}
			}

			return mask;
		}
		auto _is_visible(glm::vec3 position, float radius, bool camera_only, Culling_mask in_mask)
		        -> Culling_mask
		{
			const auto p    = glm::vec4(position, 1.f);
			auto       mask = Culling_mask(0);

			for(auto [i, f] : util::with_index(_frustums)) {
				const auto m = Culling_mask(1) << i;

				if((in_mask & m) != 0 && (!camera_only || f.is_camera) && f.visible(p, radius)) {
					mask |= m;
				}
			}

			return mask;
		}
	};

	/**
	 * Internal class that routes commands to draw objects to interested sub-passes, if they are visible
	 *
	 * Lifecycle:
	 * - pre_draw() -> call add_viewer() and prepare for handle_obj-calls
	 * - on_draw()  -> call process_sub_objs/process_obj/process_always_visible_obj
	 *		- handle_obj() of the passes is called
	 * - post_draw()-> finish work after handle_obj-calls
	 */
	template <typename... RenderPasses>
	class Object_router final : public Object_router_base {
	  private:
		using Self = Object_router<RenderPasses...>;

		// clang-format off
		template <typename... Args>
		static constexpr bool has_any_handler =
		        (detail::has_handle_obj<RenderPasses*, Frame_data&,        Culling_mask, Args&...>::value || ...) ||
		        (detail::has_handle_obj<RenderPasses*, Frame_data&, Self&, Culling_mask, Args&...>::value || ...);
		// clang-format on

	  public:
		Object_router(std::tuple<RenderPasses*...> passes) : _passes{std::move(passes)} {}

		auto active() const -> bool override
		{
			if(_frustums.empty() || _frame_data)
				return false;

			auto result = true;

			util::foreach_in_tuple(_passes, [&](auto, auto pass) mutable {
				if constexpr(detail::has_any_handle_obj<std::remove_pointer_t<decltype(pass)>>) {
					if(!pass)
						result = false;
				}
			});

			return result;
		}

		void pre_draw(Frame_data& fd) override
		{
			Object_router_base::pre_draw(fd);

			util::foreach_in_tuple(_passes, [&](auto, auto& pass) mutable {
				if constexpr(detail::has_pre_draw<decltype(pass),
				                                  Frame_data&,
				                                  Object_router<RenderPasses...>&>::value) {
					if(pass)
						pass->pre_draw(fd, *this);
				}
				if constexpr(detail::has_pre_draw<decltype(pass), Frame_data&>::value) {
					if(pass)
						pass->pre_draw(fd);
				}
			});
		}
		void post_draw(Frame_data& fd) override
		{
			util::foreach_in_tuple(_passes, [&](auto, auto& pass) mutable {
				if constexpr(detail::has_post_draw<decltype(pass),
				                                   Frame_data&,
				                                   Object_router<RenderPasses...>&>::value) {
					if(pass)
						pass->post_draw(fd, *this);
				}
				if constexpr(detail::has_post_draw<decltype(pass), Frame_data&>::value) {
					if(pass)
						pass->post_draw(fd);
				}
			});

			Object_router_base::post_draw(fd);
		}

		void on_draw(Frame_data& fd) override
		{
			util::foreach_in_tuple(_passes, [&](auto, auto& pass) mutable {
				if constexpr(detail::has_on_draw<decltype(pass),
				                                 Frame_data&,
				                                 Object_router<RenderPasses...>&>::value) {
					if(pass)
						pass->on_draw(fd, *this);
				}
				if constexpr(detail::has_on_draw<decltype(pass), Frame_data&>::value) {
					if(pass)
						pass->on_draw(fd);
				}
			});
		}

		/**
		 * Process an objects subobjects doing nested culling.
		 * e.g.
		 * process_sub_objs(main_pos, main_radius, [&](auto&& draw){
		 *		draw(sub_pos, sub_radius, transform, model);
		 *		draw(sub_pos2, sub_radius2, transform2, model2);
		 * });
		 */
		template <typename F>
		void process_sub_objs(const glm::vec3& position, float radius, bool camera_only, F&& f)
		{
			auto mask = _is_visible(position, radius, camera_only);
			if(mask != 0) {
				f(mask, [&](const glm::vec3& p, float r, auto&&... args) {
					auto m = _is_visible(position, radius, camera_only, mask);
					if(m != 0) {
						process_always_visible_obj(m, std::forward<decltype(args)>(args)...);
					}
				});
			}
		}

		template <typename... Ts>
		auto process_obj(const glm::vec3& position, float radius, bool camera_only, Ts&&... args) -> bool
		{
			static_assert(has_any_handler<Ts...>, "No handler for the draw-call in this renderer");

			auto mask = _is_visible(position, radius, camera_only);
			if(mask != 0) {
				return process_always_visible_obj(mask, std::forward<Ts>(args)...);
			}

			return false;
		}

		template <typename... Ts>
		auto process_always_visible_obj(Culling_mask mask, Ts&&... args) -> bool
		{
			static_assert(has_any_handler<Ts...>, "No handler for the draw-call in this renderer");

			auto handled     = false;
			auto any_handler = false;

			util::foreach_in_tuple(_passes, [&](auto, auto& pass) mutable {
				if constexpr(detail::has_handle_obj<decltype(pass), Frame_data&, Culling_mask, Ts&...>::value) {
					if(!handled && pass) {
						pass->handle_obj(*_frame_data, mask, args...);
						any_handler = true;
					}
				}

				if constexpr(detail::has_handle_obj<decltype(pass), Frame_data&, Self&, Culling_mask, Ts&...>::
				                     value) {
					if(!handled && pass) {
						if constexpr(std::is_same_v<bool,
						                            decltype(pass->handle_obj(
						                                    *_frame_data, *this, mask, args...))>) {
							handled |= pass->handle_obj(*_frame_data, *this, mask, args...);
							any_handler = true;
						} else {
							pass->handle_obj(*_frame_data, *this, mask, args...);
							any_handler = true;
						}
					}
				}
			});

			if(!any_handler) {
				LOG(plog::warning) << "No handle_obj for parameters: " << util::type_names<Ts...>();
			}

			return handled;
		}

	  private:
		std::tuple<RenderPasses*...> _passes;
	}; // namespace mirrage::renderer


	using Object_router_ptr = std::unique_ptr<Object_router_base>;
	using Object_router_factory =
	        std::function<Object_router_ptr(std::vector<std::unique_ptr<Render_pass>>&)>;

	template <typename... RenderPasses>
	auto make_object_router_factory()
	{
		return [](std::vector<std::unique_ptr<Render_pass>>& passes) -> Object_router_ptr {
			auto ptrs = std::tuple<RenderPasses*...>();

			util::foreach_in_tuple(ptrs, [&](auto, auto& pass_ptr_out) {
				for(auto& pass : passes) {
					pass_ptr_out = dynamic_cast<std::remove_reference_t<decltype(pass_ptr_out)>>(pass.get());
					if(pass_ptr_out)
						return;
				}
			});

			return std::make_unique<Object_router<RenderPasses...>>(ptrs);
		};
	}


} // namespace mirrage::renderer
