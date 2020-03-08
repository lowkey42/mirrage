/** small helpers for container creation an manipulation *********************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace mirrage::util {

	// helpers to erase elements from vectors
	template <typename C, typename K>
	void erase_fast(C& c, const K& v)
	{
		using std::swap;

		auto e = std::find(c.begin(), c.end(), v);
		if(e != c.end()) {
			swap(*e, c.back());
			c.pop_back();
		}
	}
	template <typename C, typename F>
	void erase_fast_if(C& c, F&& f)
	{
		using std::swap;

		auto e = std::find_if(c.begin(), c.end(), std::forward<F>(f));
		if(e != c.end()) {
			swap(*e, c.back());
			c.pop_back();
		}
	}
	template <typename C, typename K>
	void erase_fast_stable(C& c, const K& v)
	{
		auto ne = std::remove(c.begin(), c.end(), v);

		if(ne != c.end()) {
			c.erase(ne, c.end());
		}
	}

	template <typename T, typename PredicateT>
	void erase_if(std::vector<T>& items, const PredicateT& predicate)
	{
		items.erase(std::remove_if(items.begin(), items.end(), predicate), items.end());
	}

	template <typename ContainerT, typename PredicateT>
	void erase_if(ContainerT& items, const PredicateT& predicate)
	{
		for(auto it = items.begin(); it != items.end();) {
			if(predicate(*it))
				it = items.erase(it);
			else
				++it;
		}
	}


	// helpers to construct map, vectors and arrays
	template <typename C, typename F>
	auto map(C&& c, F&& f)
	{
		auto result = std::vector<decltype(f(c[0]))>();
		result.reserve(c.size());

		for(auto& e : c) {
			result.emplace_back(f(e));
		}

		return result;
	}

	template <typename... Ts>
	auto make_vector(Ts&&... values)
	{
		auto vec = std::vector<std::common_type_t<Ts...>>();
		vec.reserve(sizeof...(values));

		apply([&](auto&& value) { vec.emplace_back(std::forward<decltype(value)>(value)); },
		      std::forward<Ts>(values)...);

		return vec;
	}

	template <typename T, typename... Ts>
	auto make_array(Ts&&... values)
	{
		return std::array<T, sizeof...(Ts)>{std::forward<Ts>(values)...};
	}

	namespace detail {
		template <std::size_t N, std::size_t... I, class F>
		auto build_array_impl(F&& factory, std::index_sequence<I...>)
		{
			return std::array<std::common_type_t<decltype(factory(I))...>, N>{{factory(I)...}};
		}
	} // namespace detail

	template <std::size_t N, class F>
	auto build_array(F&& factory)
	{
		return detail::build_array_impl<N>(factory, std::make_index_sequence<N>());
	}

	template <typename T, class SizeT, class F>
	auto build_vector(SizeT n, F&& factory)
	{
		auto vec = std::vector<T>();
		vec.reserve(n);

		for(auto i = SizeT(0); i < n; i++) {
			factory(i, vec);
		}

		return vec;
	}
	template <class SizeT, class F>
	auto build_vector(SizeT n, F&& factory)
	{
		auto vec = std::vector<decltype(factory(std::declval<SizeT>()))>();
		vec.reserve(n);

		for(auto i = SizeT(0); i < n; i++) {
			vec.push_back(factory(i));
		}

		return vec;
	}

} // namespace mirrage::util
