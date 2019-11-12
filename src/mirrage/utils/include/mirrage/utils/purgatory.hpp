/** a heterogeneous container that just keeps its contents alife *************
 *                                                                           *
 * Copyright (c) 2017 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <vector>


namespace mirrage::util {

	/**
	 * @brief A heterogeneous container that can contain any moveable type and efficiantly destroys
	 *          them once its cleared or destroyed itself
	 */
	class purgatory {
	  private:
		static constexpr auto block_size = 512;

	  public:
		purgatory(std::size_t reserve = 8);
		purgatory(purgatory&&);
		purgatory& operator=(purgatory&&);
		~purgatory();

		void clear();

		/**
			 * Takes ownership of the passed object and returns a reference to it that stays valid
			 * until the container is cleared or destroyed
			 */
		template <typename T>
		auto add(T&& obj) -> T&
		{
			static_assert(sizeof(T) < block_size, "The required size is bigger than the blocksize!");
			static_assert(!std::is_lvalue_reference<T>::value,
			              "The object has to be passed as an rvalue reference to transfer its ownership!");

			auto ptr   = _reserve(sizeof(T), alignof(T));
			auto grave = new(ptr) T(std::move(obj));

			_entries.emplace_back(
			        +[](void* ptr) { reinterpret_cast<T*>(ptr)->~T(); }, grave);

			return *grave;
		}

	  private:
		using block = std::unique_ptr<char[]>;
		// deletes the pointed to object and returns its size
		using deleter_ptr = void (*)(void*);
		struct entry {
			deleter_ptr deleter;
			void*       ptr;

			entry() = default;
			entry(deleter_ptr d, void* ptr) : deleter(d), ptr(ptr) {}
		};

		std::vector<block> _blocks;
		std::size_t        _current_offset = 0;
		std::vector<entry> _entries;

		auto _reserve(std::size_t size, std::size_t alignment) -> void*;
	};
} // namespace mirrage::util
