/** small helpers for atomic programming ***********************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <atomic>
#include <vector>

namespace mirrage::util {

	namespace detail {
		template <typename T>
		class atomic_wrapper {
		  public:
			atomic_wrapper() = default;
			atomic_wrapper(T v) : _val(v) {}
			atomic_wrapper(const atomic_wrapper& rhs) noexcept : _val(rhs._val.load()) {}
			auto& operator=(const atomic_wrapper& rhs) noexcept
			{
				_val.store(rhs._val.load());
				return *this;
			}

			auto& get() const noexcept { return _val; }
			auto& get() noexcept { return _val; }

		  private:
			std::atomic<T> _val;
		};
	} // namespace detail


	/**
	 * A vector of atomics of type T
	 * Note: Any operation on the vector that copies its elements (copy, resize, ...) is not atomic
	 */
	template <typename T>
	using vector_atomic = std::vector<detail::atomic_wrapper<T>>;

	template <typename T>
	std::atomic<T>& at(vector_atomic<T>& v, std::size_t i)
	{
		return v.at(i).get();
	}
	template <typename T>
	const std::atomic<T>& at(const vector_atomic<T>& v, std::size_t i)
	{
		return v.at(i).get();
	}
} // namespace mirrage::util
