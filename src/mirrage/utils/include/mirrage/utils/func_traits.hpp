/** type traits to analyze a function/functor type ***************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <functional>
#include <iostream>
#include <typeinfo>
#include <tuple>


namespace mirrage {
namespace util {
	
	enum class func_type {
		free, member, functor
	};


	template<typename F>
	struct func_trait : func_trait<decltype(&F::operator())> {
		static constexpr auto type = func_type::functor;
	};

	template<typename Type, typename Return, typename... Arg>
	struct func_trait<Return(Type::*)(Arg...)> : func_trait<Return(*)(Arg...)> {
		static constexpr auto type = func_type::member;
	};
	template<typename Type, typename Return, typename... Arg>
	struct func_trait<Return(Type::*)(Arg...)const> : func_trait<Return(*)(Arg...)> {
		static constexpr auto type = func_type::member;
	};

	template<typename Return, typename... Arg>
	struct func_trait<Return(*)(Arg...)> {
		static constexpr auto type = func_type::free;
		using return_t = Return;
		static constexpr auto argument_count = sizeof...(Arg);

		private:
			template <std::size_t i>
			struct arg {
				static_assert(i<argument_count, "Function doesn't take that many arguments");
				using type = typename std::tuple_element<i, std::tuple<Arg...>>::type;
			};

		public:
			template <std::size_t i>
			using arg_t = typename arg<i>::type;
	};

	template<typename T, std::size_t i>
	using nth_func_arg_t = typename func_trait<std::decay_t<T>>::template arg_t<i>;

	
	template <class T, class M> M get_member_type(M T:: *);
	template <class T, class M, std::size_t S> M (&get_member_type(M (T:: *)[S]))[S];
	
	template<class P, class M>
	std::size_t get_member_offset(const M P::*member) {
		static_assert(std::is_standard_layout<P>::value && std::is_default_constructible<P>::value,
		              "The class has to be a standard layout type and provide a default constructor!");
		constexpr P dummy{};
		return reinterpret_cast<std::size_t>(&( dummy.*member)) - reinterpret_cast<std::size_t>(&dummy);
	}

	template<typename F>
	inline void apply(F&&) {}
	template<typename F, typename FirstArg, typename... Arg>
	inline void apply(F&& func, FirstArg&& first, Arg&&... arg) {
		func(std::forward<FirstArg>(first));
		util::apply(std::forward<F>(func), std::forward<Arg>(arg)...);
	}

	template<typename F>
	inline void apply2(F&&) {}

	template<typename F, typename FirstArg,typename SecondArg, typename... Arg>
	inline void apply2(F&& func, FirstArg&& first, SecondArg&& second,
	                   Arg&&... arg) {
		func(std::forward<FirstArg>(first), std::forward<SecondArg>(second));
		util::apply2(std::forward<F>(func), std::forward<Arg>(arg)...);
	}

	template<typename O, typename F>
	auto member_fptr(O* self, F&& f) -> decltype(auto) {
		if constexpr(std::is_same<void, typename func_trait<F>::return_t>::value) {
			return [self, &f](auto&&... args) -> decltype(auto) {
				std::invoke(f, self, std::forward<decltype(args)>(args)...);
			};

		} else {
			return [self, &f](auto&&... args) -> decltype(auto) {
				return std::invoke(f, self, std::forward<decltype(args)>(args)...);
			};
		}
	}

}
}

#define FOE_SELF(MEMBER_NAME) ::mirrage::util::member_fptr(this, &std::decay_t<decltype(*this)>::MEMBER_NAME)

