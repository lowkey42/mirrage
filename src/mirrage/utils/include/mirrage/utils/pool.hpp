/** a resizable, semi-contiguous pool of memory ******************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/math.hpp>
#include <mirrage/utils/sorted_vector.hpp>
#include <mirrage/utils/string_utils.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <doctest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>


namespace mirrage::util::tests {
	struct accessor;
}
namespace mirrage::util {

	template <class POOL>
	class pool_iterator;


	struct pool_value_traits {
		static constexpr int_fast32_t max_free = 0;

		static constexpr bool sorted = false;
		/// The key the elements should be sorted by
		// static constexpr auto sort_key = T::my_key;
		/// The position of the key in the constructor argument list
		// static constexpr auto sort_key_constructor_idx = 42;
	};

	/// A dynamic semi-contiguous container.
	/// The elements are stored in contiguous chunks of max. ElementsPerChunk elements.
	/// Based on ValueTraits the container is optionally sparse and/or sorted.
	///
	/// The container allows clients to keep track of its element positions by providing a relocation
	/// callback on each mutating member function with the signature:
	///   void(IndexType old, T& value, IndexType new)
	///
	/// All iterators and references are invalidated on any mutation.
	/// The behaviour is undefined if the container is sorted and the sort_key of an inserted value is modified.
	template <class T,
	          std::size_t ElementsPerChunk,
	          class ValueTraits = pool_value_traits,
	          class IndexType   = int_fast64_t>
	class pool {
		using storage_t = std::aligned_storage_t<sizeof(T), alignof(T)>;
		static_assert(
		        std::is_nothrow_move_assignable_v<
		                T> && std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>,
		        "The type T has to be no-throw move- and destructable!");

	  public:
		static constexpr auto element_size   = static_cast<IndexType>(sizeof(T));
		static constexpr auto chunk_len      = static_cast<IndexType>(ElementsPerChunk);
		static constexpr auto sorted         = ValueTraits::sorted;
		static constexpr auto max_free_slots = ValueTraits::max_free;

		using value_type = T;
		using iterator   = pool_iterator<pool<T, ElementsPerChunk, ValueTraits, IndexType>>;
		using index_t    = IndexType;

		friend iterator;
		friend struct ::mirrage::util::tests::accessor;


		pool() noexcept           = default;
		pool(pool&& rhs) noexcept = default;
		~pool() { clear(); }

		pool& operator=(pool&& rhs) noexcept;

		auto begin() noexcept -> iterator;
		auto end() noexcept -> iterator;

		/// Deletes all elements. Complexity: O(N)
		void clear() noexcept;

		/// Searches for an element based on its sort_key.
		/// find shall only participate in overload resolution if the pool is sorted.
		/// Complexity: O(log N)
		template <class Key,
		          class = std::enable_if_t<
		                  std::is_same_v<Key, decltype(std::declval<T>().*ValueTraits::sort_key)>>>
		auto find(const Key& key) -> util::maybe<index_t>;

		/// Deletes an element based on its index.
		/// The behaviour is undefined if i is not a valid index.
		/// Complexity: O(1) for spare or unsorted pools and O(N) else
		template <typename F>
		void erase(IndexType i, F&& relocation, bool leave_holes = true);

		/// Tries to compact the elements and free unused memory.
		/// Complexity: O(N) for sparse pools and O(1) else
		template <typename F>
		void shrink_to_fit(F&& relocation);

		/// Creates a new element inside the container.
		/// Complexity: O(N) for sorted pools and O(1) else
		template <typename F, class... Args>
		auto emplace(F&& relocation, Args&&... args) -> std::tuple<T&, IndexType>;

		/// Replaces the element at the given index with a new value.
		/// The behaviour is undefined if i is not a valid index or the pool is sorted and
		/// the new element has a different sort_key than the old.
		/// Complexity: O(1)
		void replace(IndexType i, T&& new_element) { get(i) = std::move(new_element); }


		IndexType size() const noexcept { return _used_elements - IndexType(_freelist.size()); }
		bool      empty() const noexcept { return size() == 0; }

		/// Returns the element at the given index.
		/// The behaviour is undefined if i is not a valid index.
		T&       get(IndexType i) { return *std::launder(reinterpret_cast<T*>(_get_raw(i))); }
		const T& get(IndexType i) const { return *std::launder(reinterpret_cast<const T*>(_get_raw(i))); }

	  protected:
		using chunk_type = std::unique_ptr<storage_t[]>;
		std::vector<chunk_type>  _chunks;
		IndexType                _used_elements = 0;
		sorted_vector<IndexType> _freelist;

		unsigned char* _get_raw(IndexType i)
		{
			return const_cast<unsigned char*>(static_cast<const pool*>(this)->_get_raw(i));
		}
		auto _get_raw(IndexType i) const -> const unsigned char*;

		auto _chunk(IndexType chunk_idx) noexcept -> T*;
		auto _chunk_end(T* begin, IndexType chunk_idx) noexcept -> T*;

		/// Moves the range [src, src+count) to [dst, dst+count), calling on_relocate accordingly
		/// The behaviour is undefined if any of the ranges contains an empty/invalid value!
		/// The non-overlapping parts of the src range will be in the moved-from state afterwards.
		template <typename F>
		void _move_elements(index_t src, index_t dst, F&& on_relocate, index_t count = 1);

		void _pop_back();
	};


	/// An iterator to the valid elements of a pool.
	template <class Pool>
	class pool_iterator {
	  public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type        = typename Pool::value_type;
		using difference_type   = std::int_fast32_t;
		using index_type        = typename Pool::index_t;
		using pointer           = value_type*;
		using reference         = value_type&;

		pool_iterator();
		pool_iterator(Pool& pool, typename Pool::index_t index);

		/// The current index of the iterator as returned by std::distance(begin(pool), iter)
		auto logical_index() const noexcept { return _logical_index; }

		/// The current index of the iterator into the pool-storage, as required by pool::get(...)
		auto physical_index() const noexcept { return _physical_index; }

		value_type& operator*() noexcept { return *get(); }
		value_type* operator->() noexcept { return get(); }
		value_type* get() noexcept;

		auto operator++() -> pool_iterator&;

		auto operator++(int) -> pool_iterator;

		auto operator--() -> pool_iterator&;

		auto operator--(int) -> pool_iterator;

		auto operator+=(difference_type n) -> pool_iterator&;
		auto operator-=(difference_type n) -> pool_iterator&;

		auto operator[](difference_type i) -> reference;


		friend auto operator-(const pool_iterator& lhs, const pool_iterator& rhs) noexcept
		{
			return static_cast<difference_type>(lhs._logical_index)
			       - static_cast<difference_type>(rhs._logical_index);
		}
		friend auto operator+(pool_iterator iter, difference_type offset)
		{
			iter += offset;
			return iter;
		}
		friend auto operator+(difference_type offset, pool_iterator iter)
		{
			iter += offset;
			return iter;
		}
		friend auto operator-(pool_iterator iter, difference_type offset)
		{
			iter -= offset;
			return iter;
		}

		friend auto operator<(const pool_iterator& lhs, const pool_iterator& rhs) noexcept
		{
			return lhs._physical_index < rhs._physical_index;
		}
		friend auto operator>(const pool_iterator& lhs, const pool_iterator& rhs) noexcept
		{
			return lhs._physical_index > rhs._physical_index;
		}
		friend auto operator<=(const pool_iterator& lhs, const pool_iterator& rhs) noexcept
		{
			return lhs._physical_index <= rhs._physical_index;
		}
		friend auto operator>=(const pool_iterator& lhs, const pool_iterator& rhs) noexcept
		{
			return lhs._physical_index >= rhs._physical_index;
		}
		friend auto operator==(const pool_iterator& lhs, const pool_iterator& rhs) noexcept
		{
			return lhs._element_iter == rhs._element_iter;
		}
		friend auto operator!=(const pool_iterator& lhs, const pool_iterator& rhs) noexcept
		{
			return !(lhs == rhs);
		}

	  private:
		using free_iterator = typename sorted_vector<index_type>::const_iterator;

		Pool*         _pool;
		index_type    _logical_index;  ///< without empty slots
		index_type    _physical_index; ///< including empty slots
		index_type    _chunk_index;
		value_type*   _element_iter;
		value_type*   _element_iter_begin;
		value_type*   _element_iter_end;
		free_iterator _next_free;
	};


} // namespace mirrage::util

#define MIRRAGE_UTIL_POOL_INCLUDED
#include "pool.hxx"


namespace mirrage::util::tests {
	struct accessor {
		std::size_t chunk_count;

		template <class Pool>
		accessor(Pool& p) : chunk_count(p._chunks.size())
		{
		}
	};

	struct T {
		int           id;
		long long int f = 42;

		T() = default;
		T(int id, long long int f) : id(id), f(f) {}
	};

	struct pool_value_traits {
		static constexpr int_fast32_t max_free = 8;

		static constexpr bool sorted                   = true;
		static constexpr auto sort_key                 = &T::id;
		static constexpr auto sort_key_constructor_idx = 0;
	};

	TEST_CASE("[Mirrage Utils] Testing pool container")
	{
		using pool_t = pool<T, 16, pool_value_traits>;

		auto p = pool_t();

		CHECK_EQ(p.begin(), p.end());

		// insertion
		{
			auto idx1 = p.emplace([](auto...) {}, 2, 2);
			CHECK_EQ(std::get<1>(idx1), 0);

			auto iter = p.begin();
			CHECK_NE(iter, p.end());
			CHECK_EQ(iter->id, 2);
			iter++;
			CHECK_EQ(iter, p.end());

			auto idx2 = p.emplace([](auto...) {}, 1, 1);
			CHECK_EQ(std::get<1>(idx2), 0);

			auto idx3 = p.emplace([](auto...) {}, 3, 3);
			CHECK_EQ(std::get<1>(idx3), 2);
		}

		// iteration
		{
			auto iter = p.begin();
			CHECK_NE(iter, p.end());
			CHECK_EQ((iter->id), 1);
			iter++;

			CHECK_EQ(iter->id, 2);
			iter++;

			CHECK_EQ(iter->id, 3);
			iter++;

			CHECK_EQ(iter, p.end());

			CHECK_EQ(p.size(), 3);
			CHECK(!p.empty());
		}

		// removal
		{
			p.erase(0, [](auto...) {});
			CHECK_EQ(p.size(), 2);
			CHECK(!p.empty());

			auto iter = p.begin();
			CHECK_EQ(iter->id, 2);
			iter++;

			CHECK_EQ(iter->id, 3);
			iter++;

			CHECK_EQ(iter, p.end());


			p.erase(1, [](auto...) {});
			CHECK_EQ(p.size(), 1);
			CHECK(!p.empty());
			p.erase(2, [](auto...) {});
			CHECK_EQ(p.size(), 0);
			CHECK(p.empty());
		}

		// shrinking
		{
			p.clear();

			auto mapping   = std::array<int, 30001>();
			auto relocator = [&](auto from, auto& v, auto to) {
				CHECK_EQ(mapping[v.id], from);
				mapping[v.id] = to;
				CHECK_EQ(v.id, p.get(to).id);
			};

			for(auto i : util::range(1, 1024)) {
				auto r = p.emplace(relocator, i, i);

				mapping[i] = std::get<1>(r);
			}

			CHECK_EQ(accessor{p}.chunk_count, 1024 / 16);

			p.erase(mapping[654], relocator);
			p.erase(mapping[3], relocator);
			p.erase(mapping[900], relocator);
			p.erase(mapping[432], relocator);

			p.emplace(relocator, 2000, 2000);
			p.emplace(relocator, 3, 3);

			p.shrink_to_fit(relocator);

			p.emplace(relocator, 3000, 3000);

			auto sum = static_cast<long long int>(0);
			for(auto& i : p) {
				sum += i.f;
			}

			CHECK_EQ(sum, 527814);
			CHECK(std::is_sorted(p.begin(), p.end(), [](auto& lhs, auto& rhs) { return lhs.id < rhs.id; }));
		}
	}
} // namespace mirrage::util::tests
