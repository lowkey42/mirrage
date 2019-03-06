/** simple wrappers for iterator pairs ***************************************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <tuple>
#include <utility>
#include <vector>

namespace mirrage::util {

	template <class Iter>
	class iter_range {
	  public:
		iter_range() noexcept {}
		iter_range(Iter begin, Iter end) noexcept : b(begin), e(end) {}

		bool operator==(const iter_range& o) noexcept { return b == o.b && e == o.e; }

		Iter begin() const noexcept { return b; }
		Iter end() const noexcept { return e; }

		std::size_t size() const noexcept { return std::distance(b, e); }

	  private:
		Iter b, e;
	};
	template <class T>
	using vector_range = iter_range<typename std::vector<T>::iterator>;

	template <class T>
	using cvector_range = iter_range<typename std::vector<T>::const_iterator>;


	template <class Range>
	class skip_range {
	  public:
		skip_range() noexcept {}
		skip_range(std::size_t skip, Range range) noexcept : _skip(skip), _range(std::move(range)) {}

		bool operator==(const skip_range& o) noexcept { return _range == o._range; }

		auto begin() const noexcept
		{
			auto i       = _range.begin();
			auto skipped = std::size_t(0);
			while(i != _range.end() && skipped++ < _skip)
				++i;
			return i;
		}
		auto end() const noexcept { return _range.end(); }

		std::size_t size() const noexcept
		{
			auto s = _range.size();
			return s > _skip ? s - _skip : 0;
		}

	  private:
		std::size_t _skip;
		Range       _range;
	};
	template <class Range>
	auto skip(std::size_t skip, Range&& range)
	{
		return skip_range<std::remove_reference_t<Range>>(skip, std::forward<Range>(range));
	}


	namespace detail {
		struct deref_join {
			template <class... Iter>
			[[maybe_unused]] auto operator()(Iter&&... iter) const
			{
				return std::tuple<decltype(*iter)...>(*iter...);
			}
		};

		template <typename T>
		class proxy_holder {
		  public:
			proxy_holder(const T& value) : value(value) {}
			T* operator->() { return &value; }
			T& operator*() { return value; }

		  private:
			T value;
		};
	} // namespace detail

	template <class... Iter>
	class join_iterator {
	  public:
		join_iterator() = default;
		template <class... T>
		join_iterator(std::size_t index, T&&... iters) : _iters(std::forward<T>(iters)...), _index(index)
		{
		}

		auto operator==(const join_iterator& rhs) const { return _index == rhs._index; }
		auto operator!=(const join_iterator& rhs) const { return !(*this == rhs); }

		auto operator*() const { return std::apply(detail::deref_join{}, _iters); }
		auto operator-> () const { return proxy_holder(**this); }

		auto operator++()
		{
			foreach_in_tuple(_iters, [](auto, auto& iter) { ++iter; });
			++_index;
		}
		auto operator++(int)
		{
			auto v = *this;
			++*this;
			return v;
		}

	  private:
		std::tuple<Iter...> _iters;
		std::size_t         _index;
	};

	namespace detail {
		struct get_join_begin {
			template <class... Range>
			[[maybe_unused]] auto operator()(Range&&... ranges) const
			{
				return join_iterator<std::remove_reference_t<decltype(ranges.begin())>...>(0,
				                                                                           ranges.begin()...);
			}
		};
		struct get_join_iterator {
			template <class... Range>
			[[maybe_unused]] auto operator()(std::size_t offset, Range&&... ranges) const
			{
				return join_iterator<std::remove_reference_t<decltype(ranges.begin())>...>(
				        offset, (ranges.begin() + offset)...);
			}
		};
	} // namespace detail

	template <class... Range>
	class join_range {
	  public:
		join_range() noexcept {}
		template <class... T>
		join_range(T&&... ranges) noexcept : _ranges(std::forward<T>(ranges)...), _size(min(ranges.size()...))
		{
		}

		auto begin() const noexcept { return std::apply(detail::get_join_begin{}, _ranges); }
		auto begin() noexcept { return std::apply(detail::get_join_begin{}, _ranges); }
		auto end() const noexcept
		{
			return std::apply(detail::get_join_iterator{}, std::tuple_cat(std::make_tuple(_size), _ranges));
		}
		auto end() noexcept
		{
			return std::apply(detail::get_join_iterator{}, std::tuple_cat(std::make_tuple(_size), _ranges));
		}

		auto size() const noexcept { return _size; }

	  private:
		std::tuple<Range...> _ranges;
		std::size_t          _size;
	};

	template <class... Range>
	auto join(Range&&... range)
	{
		return join_range<std::remove_reference_t<Range>...>(std::forward<Range>(range)...);
	}


	namespace detail {
		template <class Arg>
		auto construct_index_tuple(std::int64_t index, Arg arg)
		{
			return std::tuple<std::int64_t, Arg>(index, arg);
		}
		template <class, class... Args>
		auto construct_index_tuple(std::int64_t index, std::tuple<Args...> arg)
		{
			return std::tuple<std::int64_t, Args...>(index, std::get<Args>(arg)...);
		}


		template <class BaseIter>
		class indexing_range_iterator {
		  public:
			indexing_range_iterator(BaseIter&& iter, std::int64_t index) : iter(iter), index(index) {}

			auto operator==(const indexing_range_iterator& rhs) const { return iter == rhs.iter; }
			auto operator!=(const indexing_range_iterator& rhs) const { return !(*this == rhs); }

			auto operator*() { return construct_index_tuple<decltype(*iter)>(index, *iter); }
			auto operator-> () { return proxy_holder(**this); }

			auto operator*() const { return construct_index_tuple<decltype(*iter)>(index, *iter); }
			auto operator-> () const { return proxy_holder(**this); }

			auto operator++()
			{
				++iter;
				++index;
			}
			auto operator++(int)
			{
				auto v = *this;
				++*this;
				return v;
			}

		  private:
			BaseIter     iter;
			std::int64_t index;
		};
		template <class BaseIter>
		auto make_indexing_range_iterator(BaseIter&& iter, std::int64_t index)
		{
			return indexing_range_iterator<std::remove_reference_t<BaseIter>>(std::forward<BaseIter>(iter),
			                                                                  index);
		}
	} // namespace detail

	template <class Range>
	class indexing_range {
	  public:
		indexing_range(Range&& range) noexcept : range(std::move(range)) {}

		bool operator==(const indexing_range& rhs) noexcept { return range == rhs.range; }

		auto begin() noexcept { return detail::make_indexing_range_iterator(range.begin(), 0u); }
		auto end() noexcept { return detail::make_indexing_range_iterator(range.end(), std::int64_t(-1)); }
		auto begin() const noexcept { return detail::make_indexing_range_iterator(range.begin(), 0u); }
		auto end() const noexcept
		{
			return detail::make_indexing_range_iterator(range.end(), std::int64_t(-1));
		}

	  private:
		Range range;
	};
	template <class Range>
	auto with_index(Range&& r) -> indexing_range<std::remove_reference_t<Range>>
	{
		return {std::move(r)};
	}

	template <class Range>
	class indexing_range_view {
	  public:
		indexing_range_view(Range& range) noexcept : range(&range) {}

		bool operator==(const indexing_range_view& rhs) noexcept { return *range == *rhs.range; }

		auto begin() noexcept { return detail::make_indexing_range_iterator(range->begin(), 0u); }
		auto end() noexcept { return detail::make_indexing_range_iterator(range->end(), std::int64_t(-1)); }
		auto begin() const noexcept { return detail::make_indexing_range_iterator(range->begin(), 0u); }
		auto end() const noexcept
		{
			return detail::make_indexing_range_iterator(range->end(), std::int64_t(-1));
		}

	  private:
		Range* range;
	};
	template <class Range>
	auto with_index(Range& r) -> indexing_range_view<std::remove_reference_t<Range>>
	{
		return {r};
	}


	template <class T>
	class numeric_range {
		struct iterator : std::iterator<std::random_access_iterator_tag, T, T> {
			T p;
			T s;
			constexpr iterator(T v, T s = 1) noexcept : p(v), s(s) {}
			constexpr iterator(const iterator&) noexcept = default;
			constexpr iterator(iterator&&) noexcept      = default;
			iterator& operator++() noexcept
			{
				p += s;
				return *this;
			}
			iterator operator++(int) noexcept
			{
				auto t = *this;
				*this ++;
				return t;
			}
			iterator& operator--() noexcept
			{
				p -= s;
				return *this;
			}
			iterator operator--(int) noexcept
			{
				auto t = *this;
				*this --;
				return t;
			}
			bool     operator==(const iterator& rhs) const noexcept { return p == rhs.p; }
			bool     operator!=(const iterator& rhs) const noexcept { return p != rhs.p; }
			const T& operator*() const noexcept { return p; }
		};
		using const_iterator = iterator;

	  public:
		constexpr numeric_range() noexcept {}
		constexpr numeric_range(T begin, T end, T step = 1) noexcept : b(begin), e(end), s(step) {}
		constexpr numeric_range(numeric_range&&) noexcept      = default;
		constexpr numeric_range(const numeric_range&) noexcept = default;

		numeric_range& operator=(const numeric_range&) noexcept = default;
		numeric_range& operator=(numeric_range&&) noexcept = default;
		bool           operator==(const numeric_range& o) noexcept { return b == o.b && e == o.e; }

		constexpr iterator begin() const noexcept { return b; }
		constexpr iterator end() const noexcept { return e; }

	  private:
		T b, e, s;
	};
	template <class Iter, typename = std::enable_if_t<!std::is_arithmetic<Iter>::value>>
	constexpr iter_range<Iter> range(Iter b, Iter e)
	{
		return {b, e};
	}
	template <class B, class E, typename = std::enable_if_t<std::is_arithmetic<B>::value>>
	constexpr auto range(B b, E e, std::common_type_t<B, E> s = 1)
	{
		using T = std::common_type_t<B, E>;
		return numeric_range<T>{T(b), std::max(T(e + 1), T(b)), T(s)};
	}
	template <class T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
	constexpr numeric_range<T> range(T num)
	{
		return {0, static_cast<T>(num)};
	}
	template <class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value>>
	auto range(Container& c) -> iter_range<typename Container::iterator>
	{
		using namespace std;
		return {begin(c), end(c)};
	}
	template <class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value>>
	auto range(const Container& c) -> iter_range<typename Container::const_iterator>
	{
		using namespace std;
		return {begin(c), end(c)};
	}

	template <class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value>>
	auto range_reverse(Container& c) -> iter_range<typename Container::reverse_iterator>
	{
		using namespace std;
		return {rbegin(c), rend(c)};
	}
	template <class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value>>
	auto range_reverse(const Container& c) -> iter_range<typename Container::const_reverse_iterator>
	{
		using namespace std;
		return {rbegin(c), rend(c)};
	}

} // namespace mirrage::util
