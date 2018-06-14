/** a resizable, semi-contiguous pool of memory ******************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/string_utils.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>


namespace mirrage::util {

	template <class POOL>
	class pool_iterator;


	struct pool_value_traits {
		static constexpr bool supports_empty_values = false;
		// static constexpr int_fast32_t max_free = 8;
		// using Marker_type = Foo;
		// static constexpr Marker_type free_mark = -1;
		// static constexpr const Marker_type* marker_addr(const T* inst)
	};

	template <class T,
	          std::size_t ElementsPerChunk,
	          class IndexType       = int_fast64_t,
	          class ValueTraits     = pool_value_traits,
	          bool use_empty_values = ValueTraits::supports_empty_values>
	class pool {
		static_assert(alignof(T) <= alignof(std::max_align_t), "Alignment not supported");

	  public:
		static constexpr auto element_size = static_cast<IndexType>(sizeof(T));
		static constexpr auto chunk_len    = static_cast<IndexType>(ElementsPerChunk);
		static constexpr auto chunk_size   = chunk_len * element_size;

		using value_type = T;
		using iterator   = pool_iterator<pool<T, ElementsPerChunk, IndexType, ValueTraits, use_empty_values>>;
		using index_t    = IndexType;

		friend iterator;


		pool() noexcept       = default;
		pool(pool&&) noexcept = default;
		pool& operator=(pool&&) noexcept = default;
		~pool()                          = default;

		iterator begin() noexcept;
		iterator end() noexcept;

		/**
			 * Deletes all elements. Invalidates all iterators and references.
			 * O(N)
			 */
		void clear() noexcept
		{
			for(auto& inst : *this) {
				inst.~T();
			}

			this->_chunks.clear();
			this->_used_elements = 0;
		}
		/**
			 * Deletes the last element. Invalidates all iterators and references to the last element.
			 * O(1)
			 */
		void pop_back()
		{
			MIRRAGE_INVARIANT(_used_elements > 0, "pop_back on empty pool");
#ifdef _NDEBUG
			std::memset(get(_usedElements - 1), 0xdead, element_size);
#endif
			get(_used_elements - 1).~T();
			_used_elements--;
		}
		/**
			 * Deletes a specific element.
			 * relocation = func(original:IndexType, T& value, new:IndexType)->void
			 * Invalidates all iterators and references to the specified and the last element.
			 * O(1)
			 */
		void erase(IndexType i)
		{
			erase(i, [](auto, auto) {});
		}
		template <typename F>
		void erase(IndexType i, F&& relocation)
		{
			MIRRAGE_INVARIANT(i < _used_elements, "erase is out of range: " << i << ">=" << _used_elements);

			if(i < (_used_elements - 1)) {
				auto& pivot = back();
				relocation(_used_elements - 1, pivot, i);
				get(i) = std::move(pivot);
			}

			pop_back();
		}

		/**
			 * Frees all unused memory. May invalidate all iterators and references.
			 * relocation = func(original:IndexType, T& value, new:IndexType)->void
			 * O(1)
			 */
		void shrink_to_fit()
		{
			shrink_to_fit([](auto, auto) {});
		}
		template <typename F>
		void shrink_to_fit(F&&)
		{
			auto min_chunks = std::ceil(static_cast<float>(_used_elements) / chunk_len);
			_chunks.resize(static_cast<std::size_t>(min_chunks));
		}

		/**
			 * Creates a new instance of T inside the pool.
			 * O(1)
			 */
		template <class... Args>
		auto emplace_back(Args&&... args) -> std::tuple<T&, IndexType>
		{
			const auto i = _used_elements++;

			auto addr = [&] {
				auto chunk = i / chunk_len;

				if(chunk < static_cast<IndexType>(_chunks.size())) {
					return _chunks[chunk].get() + ((i % chunk_len) * element_size);
				} else {
					auto new_chunk = std::make_unique<unsigned char[]>(chunk_size);
					auto addr      = new_chunk.get();
					_chunks.push_back(std::move(new_chunk));
					return addr;
				}
			}();

			auto instnace = new(addr) T(std::forward<Args>(args)...);
			return {*instnace, i};
		}

		void replace(IndexType i, T&& new_element) { get(i) = std::move(new_element); }

		/**
			 * @return The number of elements in the pool
			 */
		IndexType size() const noexcept { return _used_elements; }
		bool      empty() const noexcept { return _used_elements == 0; }

		/**
			 * @return The specified element
			 */
		T&       get(IndexType i) { return *reinterpret_cast<T*>(get_raw(i)); }
		const T& get(IndexType i) const { return *reinterpret_cast<const T*>(get_raw(i)); }

		/**
			 * @return The last element
			 */
		T&       back() { return const_cast<T&>(static_cast<const pool*>(this)->back()); }
		const T& back() const
		{
			MIRRAGE_INVARIANT(_used_elements > 0, "back on empty pool");
			auto i = _used_elements - 1;
			return reinterpret_cast<const T&>(_chunks.back()[(i % chunk_len) * element_size]);
		}

	  protected:
		using chunk_type = std::unique_ptr<unsigned char[]>;
		std::vector<chunk_type> _chunks;
		IndexType               _used_elements = 0;

		// get_raw is required to avoid UB if their is no valid object at the index
		unsigned char* get_raw(IndexType i)
		{
			return const_cast<unsigned char*>(static_cast<const pool*>(this)->get_raw(i));
		}
		const unsigned char* get_raw(IndexType i) const
		{
			MIRRAGE_INVARIANT(i < _used_elements,
			                  "Pool-Index out of bounds " + to_string(i) + ">=" + to_string(_used_elements));

			return _chunks[i / chunk_len].get() + (i % chunk_len) * element_size;
		}

		T* _chunk(IndexType chunk_idx) noexcept
		{
			if(chunk_idx * chunk_len < _used_elements)
				return reinterpret_cast<T*>(_chunks.at(chunk_idx).get());
			else
				return nullptr;
		}
		T* _chunk_end(T* begin, IndexType chunk_idx) noexcept
		{
			if(chunk_idx < _used_elements / chunk_len) {
				return begin + chunk_len;
			} else {
				return begin + (_used_elements % chunk_len);
			}
		}
		static bool _valid(const T*) noexcept { return true; }
	};

	template <class T, std::size_t ElementsPerChunk, class IndexType, class ValueTraits>
	class pool<T, ElementsPerChunk, IndexType, ValueTraits, true>
	  : public pool<T, ElementsPerChunk, IndexType, ValueTraits, false> {

		using base_t = pool<T, ElementsPerChunk, IndexType, ValueTraits, false>;

	  public:
		using iterator = pool_iterator<pool<T, ElementsPerChunk, IndexType, ValueTraits, true>>;

		friend iterator;

		iterator begin() noexcept;
		iterator end() noexcept;

		IndexType size() const noexcept { return this->_used_elements - _freelist.size(); }
		bool      empty() const noexcept { return size() == 0; }

		void clear() noexcept
		{
			for(auto& inst : *this) {
				inst.~T();
			}

			this->_chunks.clear();
			this->_used_elements = 0;

			_freelist.clear();
		}

		auto erase(IndexType i)
		{
			erase(i, [](auto, auto) {});
		}
		template <typename F>
		auto erase(IndexType i, F&&)
		{
			T&   instance      = this->get(i);
			auto instance_addr = &instance;
			MIRRAGE_INVARIANT(_valid(instance_addr), "double free");

			if(i >= (this->_used_elements - 1)) {
				this->pop_back();
			} else {
				instance.~T();
				set_free(instance_addr);

				_freelist.emplace_back(i);
			}
		}

		template <class... Args>
		auto emplace_back(Args&&... args) -> std::tuple<T&, IndexType>
		{
			if(!_freelist.empty()) {
				auto i = _freelist.back();
				_freelist.pop_back();

				auto instance_addr = reinterpret_cast<T*>(this->get_raw(i));
				MIRRAGE_INVARIANT(!_valid(instance_addr), "Freed object is not marked as free");
				auto instance = (new(instance_addr) T(std::forward<Args>(args)...));
				return {*instance, i};
			}

			return base_t::emplace_back(std::forward<Args>(args)...);
		}

		void shrink_to_fit()
		{
			shrink_to_fit([](auto, auto) {});
		}
		template <typename F>
		void shrink_to_fit(F&& relocation)
		{
			if(_freelist.size() > ValueTraits::max_free) {
				std::sort(_freelist.begin(), _freelist.end(), std::greater<>{});
				for(auto i : _freelist) {
					base_t::erase(i, relocation);
				}
				_freelist.clear();
			}

			base_t::shrink_to_fit(relocation);
		}


	  protected:
		std::vector<IndexType> _freelist;

		static auto& get_marker(const T* obj) noexcept { return *ValueTraits::marker_addr(obj); }
		static void  set_free(const T* obj) noexcept
		{
			const_cast<typename ValueTraits::Marker_type&>(get_marker(obj)) = ValueTraits::free_mark;
			MIRRAGE_INVARIANT(!_valid(obj), "set_free failed");
		}
		static bool _valid(const T* obj) noexcept
		{
			if(obj == nullptr)
				return true;

			return get_marker(obj) != ValueTraits::free_mark;
		}
	};


	template <class Pool>
	class pool_iterator : public std::iterator<std::bidirectional_iterator_tag, typename Pool::value_type> {
	  public:
		using value_type = typename Pool::value_type;

		pool_iterator(Pool& pool)
		  : _pool(&pool)
		  , _chunk_index(pool._chunks.size())
		  , _element_iter(nullptr)
		  , _element_iter_begin(nullptr)
		  , _element_iter_end(nullptr)
		{
		}

		pool_iterator(Pool& pool, typename Pool::index_t index)
		  : _pool(&pool)
		  , _chunk_index(0)
		  , _element_iter(pool._chunk(0))
		  , _element_iter_begin(_element_iter)
		  , _element_iter_end(pool._chunk_end(_element_iter_begin, 0))
		{

			if(_element_iter) {
				if(!Pool::_valid(_element_iter)) {
					++*this; // jump to first valid element
				}

				// skip the first 'index' elements
				for(auto i = static_cast<typename Pool::index_t>(0); i < index; i++) {
					++*this;
				}
			}
		}

		value_type& operator*() noexcept
		{
			MIRRAGE_INVARIANT(Pool::_valid(_element_iter), "access to invalid pool_iterator");
			return *_element_iter;
		}
		value_type* operator->() noexcept
		{
			MIRRAGE_INVARIANT(Pool::_valid(_element_iter), "access to invalid pool_iterator");
			return _element_iter;
		}

		pool_iterator& operator++()
		{
			MIRRAGE_INVARIANT(_element_iter != nullptr, "iterator overflow");
			do {
				++_element_iter;
				if(_element_iter == _element_iter_end) {
					++_chunk_index;
					_element_iter_begin = _element_iter = _pool->_chunk(_chunk_index);
					_element_iter_end = _pool->_chunk_end(_element_iter_begin, _chunk_index);
				}
			} while(!Pool::_valid(_element_iter));

			return *this;
		}

		pool_iterator operator++(int)
		{
			pool_iterator t = *this;
			++*this;
			return t;
		}

		pool_iterator& operator--()
		{
			do {
				if(_element_iter == _element_iter_begin) {
					MIRRAGE_INVARIANT(_chunk_index > 0, "iterator underflow");
					--_chunk_index;
					_element_iter_begin = _pool->_chunk(_chunk_index);
					_element_iter_end   = _pool->_chunk_end(_element_iter_begin, _chunk_index);

					if(_element_iter_end != _element_iter_begin) {
						_element_iter = _element_iter_end - 1;
					} else {
						_element_iter = _element_iter_begin;
					}
				} else {
					--_element_iter;
				}
			} while(!Pool::_valid(_element_iter));

			return *this;
		}

		pool_iterator operator--(int)
		{
			pool_iterator t = *this;
			--*this;
			return t;
		}


		bool operator==(const pool_iterator& o) const { return _element_iter == o._element_iter; }
		bool operator!=(const pool_iterator& o) const { return !(*this == o); }

	  private:
		Pool*                  _pool;
		typename Pool::index_t _chunk_index;
		value_type*            _element_iter;
		value_type*            _element_iter_begin;
		value_type*            _element_iter_end;
	};

	template <class T, std::size_t ElementsPerChunk, class Index_type, class ValueTraits, bool use_empty_values>
	auto pool<T, ElementsPerChunk, Index_type, ValueTraits, use_empty_values>::begin() noexcept -> iterator
	{
		return iterator{*this, 0};
	}

	template <class T, std::size_t ElementsPerChunk, class Index_type, class ValueTraits, bool use_empty_values>
	auto pool<T, ElementsPerChunk, Index_type, ValueTraits, use_empty_values>::end() noexcept -> iterator
	{
		return iterator{*this};
	}


	template <class T, std::size_t ElementsPerChunk, class Index_type, class ValueTraits>
	auto pool<T, ElementsPerChunk, Index_type, ValueTraits, true>::begin() noexcept -> iterator
	{
		return iterator{*this, 0};
	}

	template <class T, std::size_t ElementsPerChunk, class Index_type, class ValueTraits>
	auto pool<T, ElementsPerChunk, Index_type, ValueTraits, true>::end() noexcept -> iterator
	{
		return iterator{*this};
	}
} // namespace mirrage::util
