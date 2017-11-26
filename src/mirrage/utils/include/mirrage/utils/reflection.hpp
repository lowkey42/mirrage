/** provides compile & runtime type-information ******************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <string>
#include <type_traits>
#include <typeinfo>

namespace mirrage::util {

	extern std::string demangle(const char* name);

	template <class T>
	std::string type_name() {
		return demangle(typeid(T).name());
	}

	using type_uid_t = int32_t;

	constexpr auto no_type_uid = type_uid_t(0);

	namespace details {
		struct Type_uid_gen_base {
		  protected:
			static auto next_uid() noexcept {
				static auto idc = type_uid_t(1);
				return idc++;
			}
		};
		template <typename T>
		struct Type_uid_gen : Type_uid_gen_base {
			static auto uid() noexcept {
				static auto i = next_uid();
				return i;
			}
		};
	} // namespace details

	template <class T>
	auto type_uid_of() {
		return details::Type_uid_gen<std::decay_t<std::remove_pointer_t<std::decay_t<T>>>>::uid();
	}
	template <>
	inline constexpr auto type_uid_of<void>() {
		return no_type_uid;
	}
} // namespace mirrage::util
