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
	std::string typeName() {
		return demangle(typeid(T).name());
	}

	using Typeuid = int32_t;

	constexpr auto notypeuid = Typeuid(0);

	namespace details {
		struct Typeuid_gen_base {
		  protected:
			static auto next_uid() noexcept {
				static auto idc = Typeuid(1);
				return idc++;
			}
		};
		template <typename T>
		struct Typeuid_gen : Typeuid_gen_base {
			static auto uid() noexcept {
				static auto i = next_uid();
				return i;
			}
		};
	} // namespace details

	template <class T>
	auto typeuid_of() {
		return details::Typeuid_gen<std::decay_t<std::remove_pointer_t<std::decay_t<T>>>>::uid();
	}
	template <>
	inline constexpr auto typeuid_of<void>() {
		return notypeuid;
	}
} // namespace mirrage::util
