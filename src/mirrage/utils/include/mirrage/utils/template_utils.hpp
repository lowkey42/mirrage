/** small helpers for template programming ***********************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/func_traits.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/min_max.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace mirrage::util {

	template <typename T>
	constexpr bool dependent_false()
	{
		return false;
	}

	template <typename T>
	T identity(T t)
	{
		return t;
	}


	// for deduction idiom
	template <typename... Ts>
	using void_t = void;

	namespace detail {
		template <typename, template <typename...> class, typename...>
		struct is_detected : std::false_type {
		};

		template <template <class...> class Operation, typename... Arguments>
		struct is_detected<void_t<Operation<Arguments...>>, Operation, Arguments...> : std::true_type {
		};
	} // namespace detail

	template <template <class...> class Operation, typename... Arguments>
	using is_detected = detail::is_detected<void_t<>, Operation, Arguments...>;

	template <template <class...> class Operation, typename... Arguments>
	constexpr bool is_detected_v = detail::is_detected<void_t<>, Operation, Arguments...>::value;


	// CTMP-List from https://codereview.stackexchange.com/questions/115740/filtering-variadic-template-arguments
	template <typename...>
	struct list {
	};

	namespace detail {
		template <typename, typename>
		struct list_append_impl;

		template <typename... Ts, typename... Us>
		struct list_append_impl<list<Ts...>, list<Us...>> {
			using type = list<Ts..., Us...>;
		};

		template <template <typename> class, typename...>
		struct filter_impl;

		template <template <typename> class Predicate>
		struct filter_impl<Predicate> {
			using type = list<>;
		};

		template <template <typename> class Predicate, typename T, typename... Rest>
		struct filter_impl<Predicate, T, Rest...> {
			using type = typename list_append_impl<std::conditional_t<Predicate<T>::value, list<T>, list<>>,
			                                       typename filter_impl<Predicate, Rest...>::type>::type;
		};

		template <template <typename> class Predicate, typename... Ts>
		auto filter_helper_list(list<Ts...>) -> typename filter_impl<Predicate, Ts...>::type;
	} // namespace detail

	template <template <typename> class Predicate, typename... Ts>
	using filter = typename detail::filter_impl<Predicate, Ts...>::type;

	template <template <typename> class Predicate, typename List>
	using filter_list = decltype(detail::filter_helper_list<Predicate>(std::declval<List>()));

	template <typename T>
	struct Type_wrapper {
		using type = T;
	};

	template <typename F, typename... Ts>
	void foreach_type(list<Ts...>, F&& consumer)
	{
		(consumer(Type_wrapper<Ts>{}), ...);
	}

	template <template <typename...> class T, typename>
	struct instanciate_from_type_list;
	template <template <typename...> class T, typename... Ts>
	struct instanciate_from_type_list<T, list<Ts...>> {
		using type = T<Ts...>;
	};


	// ON_EXIT {...}; (mainly for interaction with C-Code)
	template <class Func>
	struct cleanup {
		cleanup(Func f) noexcept : active(true), f(f) {}
		cleanup(const cleanup&) = delete;
		cleanup(cleanup&& o) noexcept : active(o.active), f(o.f) { o.active = false; }
		~cleanup() noexcept { f(); }

		bool active;
		Func f;
	};
	template <class Func>
	inline auto cleanup_later(Func&& f) noexcept
	{
		return cleanup<Func>{std::forward<Func>(f)};
	}

	namespace detail {
		enum class cleanup_scope_guard {};
		template <class Func>
		auto operator+(cleanup_scope_guard, Func&& f)
		{
			return cleanup<Func>{std::forward<Func>(f)};
		}
	} // namespace detail

#define CLEANUP_CONCATENATE_DIRECT(s1, s2) s1##s2
#define CLEANUP_CONCATENATE(s1, s2) CLEANUP_CONCATENATE_DIRECT(s1, s2)
#define CLEANUP_ANONYMOUS_VARIABLE(str) CLEANUP_CONCATENATE(str, __LINE__)

#define ON_EXIT \
	auto CLEANUP_ANONYMOUS_VARIABLE(_on_scope_exit) = ::mirrage::util::detail::cleanup_scope_guard() + [&]



	// non-threadsave function that calls its argument exactly once (mainly for debugging)
	template <typename F>
	void doOnce(F f)
	{
		static bool first = true;
		if(first) {
			first = false;
			f();
		}
	}


	namespace detail {
		using type_id_t = std::intptr_t;

		template <typename T>
		struct type {
			static void id() {}
		};

		template <typename T>
		type_id_t type_id()
		{
			return reinterpret_cast<type_id_t>(&type<T>::id);
		}
	} // namespace detail

	// simple any type for pointers as a type-safe alternative to void*
	class any_ptr {
	  public:
		any_ptr() : _ptr(nullptr), _type_id(0) {}
		any_ptr(std::nullptr_t) : _ptr(nullptr), _type_id(0) {}
		template <typename T>
		any_ptr(T* ptr) : _ptr(ptr), _type_id(detail::type_id<T>())
		{
		}

		any_ptr& operator=(std::nullptr_t)
		{
			_ptr     = nullptr;
			_type_id = 0;
			return *this;
		}
		template <typename T>
		any_ptr& operator=(T* ptr)
		{
			_ptr     = ptr;
			_type_id = detail::type_id<T>();
			return *this;
		}

		template <typename T>
		auto try_extract() const -> T*
		{
			if(detail::type_id<T>() == _type_id)
				return static_cast<T*>(_ptr);
			else
				return nullptr;
		}

	  private:
		void*             _ptr;
		detail::type_id_t _type_id;
	};

} // namespace mirrage::util
