/** variadic min/max implementation ******************************************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <type_traits>
#include <utility>


namespace mirrage::util {

	namespace details {
		template <typename... Ts>
		struct min_max_result {
			using common_type    = std::common_type_t<Ts...>;
			using no_temporaries = std::conjunction<std::is_lvalue_reference<Ts>...>;
			using reference_allowed =
			        std::conjunction<std::is_convertible<std::add_lvalue_reference_t<Ts>,
			                                             std::add_lvalue_reference_t<common_type>>...>;

			using use_references = std::conjunction<no_temporaries, reference_allowed>;

			using type = std::conditional_t<use_references::value,
			                                std::add_lvalue_reference_t<common_type>,
			                                common_type>;
		};

		template <typename... Ts>
		using min_max_result_t = typename min_max_result<Ts...>::type;
	} // namespace details

	// min
	template <typename FirstT>
	constexpr auto min(FirstT&& first) -> FirstT&&
	{
		return static_cast<FirstT&&>(first);
	}

	template <typename FirstT, typename SecondT>
	constexpr auto min(FirstT&& first, SecondT&& second) -> details::min_max_result_t<FirstT, SecondT>
	{
		using result_t = details::min_max_result_t<FirstT, SecondT>;

		if(static_cast<result_t>(first) < static_cast<result_t>(second))
			return static_cast<result_t>(first);
		else
			return static_cast<result_t>(second);
	}

	template <typename FirstT, typename SecondT, typename... Ts>
	constexpr auto min(FirstT&& first, SecondT&& second, Ts&&... rest)
	        -> details::min_max_result_t<FirstT, SecondT, Ts...>
	{
		return min(min(std::forward<FirstT>(first), std::forward<SecondT>(second)),
		           std::forward<Ts>(rest)...);
	}

	// max
	template <typename FirstT>
	constexpr auto max(FirstT&& first) -> FirstT&&
	{
		return static_cast<FirstT&&>(first);
	}

	template <typename FirstT, typename SecondT>
	constexpr auto max(FirstT&& first, SecondT&& second) -> details::min_max_result_t<FirstT, SecondT>
	{
		using result_t = details::min_max_result_t<FirstT, SecondT>;

		if(static_cast<result_t>(second) < static_cast<result_t>(first))
			return static_cast<result_t>(first);
		else
			return static_cast<result_t>(second);
	}

	template <typename FirstT, typename SecondT, typename... Ts>
	constexpr auto max(FirstT&& first, SecondT&& second, Ts&&... rest)
	        -> details::min_max_result_t<FirstT, SecondT, Ts...>
	{
		return max(max(std::forward<FirstT>(first), std::forward<SecondT>(second)),
		           std::forward<Ts>(rest)...);
	}

} // namespace mirrage::util
