#pragma once

#ifndef MIRRAGE_UTIL_POOL_INCLUDED
#include "pool.hpp"
#endif

namespace mirrage::util {

#define MIRRAGE_POOL_HEADER \
	template <class T, std::size_t ElementsPerChunk, class ValueTraits, class IndexType>
#define MIRRAGE_POOL pool<T, ElementsPerChunk, ValueTraits, IndexType>


	MIRRAGE_POOL_HEADER
	auto MIRRAGE_POOL::operator=(pool&& rhs) noexcept -> pool&
	{
		if(&rhs != this)
			clear();

		_chunks        = std::move(rhs._chunks);
		_used_elements = std::move(rhs._used_elements);
		_freelist      = std::move(rhs._freelist);

		return *this;
	}

	MIRRAGE_POOL_HEADER
	auto MIRRAGE_POOL::begin() noexcept -> iterator { return iterator{*this, 0}; }

	MIRRAGE_POOL_HEADER
	auto MIRRAGE_POOL::end() noexcept -> iterator { return iterator{*this, size()}; }

	MIRRAGE_POOL_HEADER
	void MIRRAGE_POOL::clear() noexcept
	{
		for(auto& inst : *this) {
			inst.~T();
		}

		_chunks.clear();
		_used_elements = 0;
		_freelist.clear();
	}

	MIRRAGE_POOL_HEADER
	template <class Key, class>
	auto MIRRAGE_POOL::find(const Key& key) -> util::maybe<index_t>
	{
		auto iter = std::lower_bound(
		        begin(), end(), key, [](auto& lhs, auto& rhs) { return lhs.*(ValueTraits::sort_key) < rhs; });

		if(iter == end())
			return util::nothing;
		else
			return iter.index();
	}

	MIRRAGE_POOL_HEADER
	template <typename F>
	void MIRRAGE_POOL::erase(IndexType i, F&& relocation, bool leave_holes)
	{
		MIRRAGE_INVARIANT(i < _used_elements, "erase is out of range: " << i << ">=" << _used_elements);

		if(i == _used_elements - 1) {
			_pop_back();

		} else {
			if constexpr(max_free_slots > 0) {
				if(leave_holes) {
					// empty slot allowed => leave a hole
					auto& e = get(i);
					e.~T();
					std::memset(reinterpret_cast<char*>(&e), 0, sizeof(T));
					_freelist.insert(i);
					return;
				}
			}

			if constexpr(ValueTraits::sorted) {
				// TODO[optimization]: _move_elements could be called with a larger range

				// shift all later elements and delete the last (now empty)
				_move_elements(i + 1, i, relocation, _used_elements - i - 1);
				_pop_back();

			} else {
				// swap with last and pop_back
				if(i < _used_elements - 1) {
					auto& pivot = get(_used_elements - 1);
					relocation(_used_elements - 1, pivot, i);
					get(i) = std::move(pivot);
				}
				_pop_back();
			}
		}
	}

	MIRRAGE_POOL_HEADER
	template <typename F>
	void MIRRAGE_POOL::shrink_to_fit(F&& relocation)
	{
		if constexpr(max_free_slots > 0) {
			if(_freelist.size() > max_free_slots) {
				if constexpr(ValueTraits::sorted) {
					// find first free index
					auto free_iter      = _freelist.begin();
					auto write_position = *free_iter++;
					auto read_position  = write_position + 1;

					while(read_position < _used_elements) {
						// skip all free slots
						while(free_iter != _freelist.end() && read_position == *free_iter) {
							free_iter++;
							read_position++;
						}

						// count non-free slots
						auto next_free  = (free_iter == _freelist.end()) ? _used_elements : *free_iter;
						auto block_size = next_free - read_position;

						// move block of non-free slots to insert_position
						_move_elements(read_position, write_position, relocation, block_size);
						read_position += block_size;
						write_position += block_size;
					}

					_used_elements = write_position;

				} else {
					for(auto i : range_reverse(_freelist)) {
						erase(i, relocation, false);
					}
				}

				_freelist.clear();
			}
		}

		// free unused chunks
		auto min_chunks = std::ceil(static_cast<float>(_used_elements) / chunk_len);
		_chunks.resize(static_cast<std::size_t>(min_chunks));
	}

	MIRRAGE_POOL_HEADER
	template <typename F, class... Args>
	auto MIRRAGE_POOL::emplace(F&& relocation, Args&&... args) -> std::tuple<T&, IndexType>
	{
		using std::swap;

		auto addr = [&](auto index) {
			auto chunk = index / chunk_len;

			if(chunk < static_cast<IndexType>(_chunks.size())) {
				return _chunks[std::size_t(chunk)].get() + (index % chunk_len);
			} else {
				return _chunks.emplace_back(std::make_unique<storage_t[]>(chunk_len)).get();
			}
		};


		auto i = _used_elements;

		if constexpr(ValueTraits::sorted) {
			auto sort_key = [&] {
				using first_arg_type =
				        std::remove_cv_t<std::remove_reference_t<std::tuple_element_t<0, std::tuple<Args...>>>>;

				if constexpr(sizeof...(args) == 1 && std::is_same_v<T, first_arg_type>) {
					// copy/move construction
					auto&& first_arg = std::get<0>(std::forward_as_tuple(args...));
					return first_arg.*(ValueTraits::sort_key);

				} else {
					// normal constructor call
					return std::get<ValueTraits::sort_key_constructor_idx>(std::tie(args...));
				}
			}();

			// find insert position
			auto iter = std::lower_bound(begin(), end(), sort_key, [](auto& lhs, auto& rhs) {
				return lhs.*(ValueTraits::sort_key) < rhs;
			});

			// shift to make room
			if(iter != end()) {
				// calc insert index
				i = iter.physical_index();

				// find first free slot
				auto first_empty = [&] {
					if constexpr(max_free_slots > 0) {
						auto min = std::lower_bound(_freelist.begin(), _freelist.end(), i - 1);
						if(min != _freelist.end()) {
							auto min_v = *min;
							_freelist.erase(min);
							return min_v;
						}
					}

					// create new slot if required
					addr(_used_elements);
					_used_elements++;

					return _used_elements - 1;
				}();

				if(first_empty < i)
					i = first_empty;

				if(first_empty > i) {
					new(_get_raw(first_empty)) T(std::move(get(first_empty - 1)));

					// shift to make room for new element
					if(first_empty - i > 1) {
						_move_elements(i, i + 1, relocation, first_empty - i - 1);
					}

					iter->~T();
				}

			} else {
				// insert at the end
				_used_elements++;
			}

		} else if constexpr(max_free_slots > 0) {
			// check free-list first
			if(!_freelist.empty()) {
				i = _freelist.pop_back();
			} else {
				_used_elements++;
			}
		}

		// create new element
		auto instance  = new(addr(i)) T(std::forward<Args>(args)...);
		auto instance2 = instance + 1;

		(void) instance2;

		return {*instance, i};
	}

	MIRRAGE_POOL_HEADER
	const unsigned char* MIRRAGE_POOL::_get_raw(IndexType i) const
	{
		MIRRAGE_INVARIANT(i < _used_elements,
		                  "Pool-Index out of bounds " + to_string(i) + ">=" + to_string(_used_elements));

		return reinterpret_cast<const unsigned char*>(_chunks[std::size_t(i / chunk_len)].get()
		                                              + (i % chunk_len));
	}

	MIRRAGE_POOL_HEADER
	T* MIRRAGE_POOL::_chunk(IndexType chunk_idx) noexcept
	{
		if(chunk_idx * chunk_len < _used_elements)
			return reinterpret_cast<T*>(_chunks.at(std::size_t(chunk_idx)).get());
		else
			return nullptr;
	}

	MIRRAGE_POOL_HEADER
	T* MIRRAGE_POOL::_chunk_end(T* begin, IndexType chunk_idx) noexcept
	{
		if(chunk_idx < _used_elements / chunk_len) {
			return begin + chunk_len;
		} else {
			return begin + (_used_elements % chunk_len);
		}
	}

	MIRRAGE_POOL_HEADER
	template <typename F>
	void MIRRAGE_POOL::_move_elements(index_t src, index_t dst, F&& on_relocate, index_t count)
	{
		auto c_src = src;
		auto c_dst = dst;
		while(c_src - src < count) {
			auto step = util::min(count, chunk_len - c_src % chunk_len, chunk_len - c_dst % chunk_len);
			if constexpr(std::is_trivially_copyable_v<T>) {
				// yay, we can memmove
				std::memmove(_get_raw(c_dst), _get_raw(c_src), std::size_t(step));
			} else {
				// nay, have to check if valid and call move-assignment / placement-new
				if(c_dst > c_src) {
					std::move_backward(&get(c_src), &get(c_src) + step, &get(c_dst) + step);
				} else {
					std::move(&get(c_src), &get(c_src) + step, &get(c_dst));
				}
			}
			c_src += step;
			c_dst += step;
		}

		for(auto i : util::range(count)) {
			on_relocate(src + i, get(dst + i), dst + i);
		}
	}

	MIRRAGE_POOL_HEADER
	void MIRRAGE_POOL::_pop_back()
	{
		MIRRAGE_INVARIANT(_used_elements > 0, "pop_back on empty pool");
#ifdef _NDEBUG
		std::memset(get(_usedElements - 1), 0xdead, element_size);
#endif
		get(_used_elements - 1).~T();
		_used_elements--;
	}

#undef MIRRAGE_POOL_HEADER
#undef MIRRAGE_POOL


	// ITERATOR IMPL

	template <class Pool>
	pool_iterator<Pool>::pool_iterator()
	  : _pool(nullptr)
	  , _chunk_index(0)
	  , _element_iter(nullptr)
	  , _element_iter_begin(nullptr)
	  , _element_iter_end(nullptr)
	{
	}

	template <class Pool>
	pool_iterator<Pool>::pool_iterator(Pool& pool, typename Pool::index_t index)
	  : _pool(&pool)
	  , _logical_index(0)
	  , _physical_index(0)
	  , _chunk_index(0)
	  , _element_iter(pool._chunk(0))
	  , _element_iter_begin(_element_iter)
	  , _element_iter_end(pool._chunk_end(_element_iter_begin, 0))
	  , _next_free(pool._freelist.begin())
	{

		if(index >= pool.size() || !_element_iter) {
			_logical_index  = pool.size();
			_physical_index = pool._used_elements;
			_element_iter   = nullptr;

		} else {
			if(_next_free != pool._freelist.end() && *_next_free == 0) {
				++_next_free;
				++*this; // jump to first valid element
			}

			_logical_index = 0;

			// skip the first 'index' elements
			*this += index;
		}
	}

	template <class Pool>
	auto pool_iterator<Pool>::get() noexcept -> value_type*
	{
		MIRRAGE_INVARIANT(_element_iter, "access to invalid pool_iterator");
		return std::launder(_element_iter);
	}

	template <class Pool>
	auto pool_iterator<Pool>::operator++() -> pool_iterator&
	{
		MIRRAGE_INVARIANT(_element_iter != nullptr, "iterator overflow");

		auto failed = false;
		do {
			++_element_iter;
			++_physical_index;
			if(_element_iter == _element_iter_end) {
				++_chunk_index;
				_element_iter_begin = _element_iter = _pool->_chunk(_chunk_index);
				_element_iter_end                   = _pool->_chunk_end(_element_iter_begin, _chunk_index);
			}

			if(failed && _next_free != _pool->_freelist.end()) {
				_next_free++;
			}
			failed = true;
		} while(_next_free != _pool->_freelist.end() && *_next_free == _physical_index);

		++_logical_index;

		return *this;
	}

	template <class Pool>
	auto pool_iterator<Pool>::operator++(int) -> pool_iterator
	{
		pool_iterator t = *this;
		++*this;
		return t;
	}

	template <class Pool>
	auto pool_iterator<Pool>::operator--() -> pool_iterator&
	{
		auto failed = false;
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

			--_physical_index;

			if(failed && _next_free != _pool->_freelist.end()) {
				_next_free++;
			}
			failed = true;
		} while(_next_free != _pool->_freelist.end() && *_next_free == _physical_index);

		--_logical_index;

		return *this;
	}

	template <class Pool>
	auto pool_iterator<Pool>::operator--(int) -> pool_iterator
	{
		pool_iterator t = *this;
		--*this;
		return t;
	}

	template <class Pool>
	auto pool_iterator<Pool>::operator+=(difference_type n) -> pool_iterator&
	{
		// compute physical_index, skipping empty slots
		auto target_physical_idx = index_type(physical_index() + n);

		while(_next_free != _pool->_freelist.end() && *_next_free <= target_physical_idx) {
			target_physical_idx++;
			_next_free++;
		}

		// compute chunk and pointers based on new physical index
		_logical_index      = index_type(_logical_index + n);
		_physical_index     = target_physical_idx;
		_chunk_index        = target_physical_idx / Pool::chunk_len;
		_element_iter_begin = _pool->_chunk(_chunk_index);
		_element_iter_end   = _pool->_chunk_end(_element_iter_begin, _chunk_index);
		_element_iter       = _element_iter_begin + (target_physical_idx % Pool::chunk_len);

		if(target_physical_idx >= _pool->_used_elements) {
			*this = _pool->end();
		}

		return *this;
	}

	template <class Pool>
	auto pool_iterator<Pool>::operator-=(difference_type n) -> pool_iterator&
	{
		*this += -n;
		return *this;
	}

	template <class Pool>
	auto pool_iterator<Pool>::operator[](difference_type i) -> reference
	{
		auto iter = *this;
		iter += i;
		return *iter;
	}

} // namespace mirrage::util
