/** small helpers for template programming ***********************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/func_traits.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/math.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace mirrage::util {

	struct no_move {
	  protected:
		no_move()           = default;
		~no_move() noexcept = default;
		no_move(no_move&&)  = delete;
		no_move& operator=(no_move&&) = delete;
	};
	struct no_copy {
	  protected:
		no_copy()               = default;
		~no_copy() noexcept     = default;
		no_copy(const no_copy&) = delete;
		no_copy& operator=(const no_copy&) = delete;
	};
	struct no_copy_move : no_copy, no_move {
	  protected:
		no_copy_move()           = default;
		~no_copy_move() noexcept = default;
	};

	template <class Func>
	struct cleanup {
		cleanup(Func f) noexcept : active(true), f(f) {}
		cleanup(const cleanup&) = delete;
		cleanup(cleanup&& o) noexcept : active(o.active), f(o.f) { o.active = false; }
		~cleanup() noexcept { f(); }

		bool active;
		Func f;
	};
	template <class Func>
	inline auto cleanup_later(Func&& f) noexcept
	{
		return cleanup<Func>{std::forward<Func>(f)};
	}

	namespace detail {
		enum class cleanup_scope_guard {};
		template <class Func>
		auto operator+(cleanup_scope_guard, Func&& f)
		{
			return cleanup<Func>{std::forward<Func>(f)};
		}
	} // namespace detail

#define CLEANUP_CONCATENATE_DIRECT(s1, s2) s1##s2
#define CLEANUP_CONCATENATE(s1, s2) CLEANUP_CONCATENATE_DIRECT(s1, s2)
#define CLEANUP_ANONYMOUS_VARIABLE(str) CLEANUP_CONCATENATE(str, __LINE__)

#define ON_EXIT \
	auto CLEANUP_ANONYMOUS_VARIABLE(_on_scope_exit) = ::mirrage::util::detail::cleanup_scope_guard() + [&]


	template <typename T>
	constexpr bool dependent_false()
	{
		return false;
	}

	template <typename T>
	T identity(T t)
	{
		return t;
	}

	template <typename... Ts>
	using void_t = void;

	namespace detail {
		template <typename, template <typename...> class, typename...>
		struct is_detected : std::false_type {
		};

		template <template <class...> class Operation, typename... Arguments>
		struct is_detected<void_t<Operation<Arguments...>>, Operation, Arguments...> : std::true_type {
		};
	} // namespace detail

	template <template <class...> class Operation, typename... Arguments>
	using is_detected = detail::is_detected<void_t<>, Operation, Arguments...>;

	template <template <class...> class Operation, typename... Arguments>
	constexpr bool is_detected_v = detail::is_detected<void_t<>, Operation, Arguments...>::value;


	// CTMP-List from https://codereview.stackexchange.com/questions/115740/filtering-variadic-template-arguments
	template <typename...>
	struct list {
	};

	namespace detail {
		template <typename, typename>
		struct list_append_impl;

		template <typename... Ts, typename... Us>
		struct list_append_impl<list<Ts...>, list<Us...>> {
			using type = list<Ts..., Us...>;
		};

		template <template <typename> class, typename...>
		struct filter_impl;

		template <template <typename> class Predicate>
		struct filter_impl<Predicate> {
			using type = list<>;
		};

		template <template <typename> class Predicate, typename T, typename... Rest>
		struct filter_impl<Predicate, T, Rest...> {
			using type = typename list_append_impl<std::conditional_t<Predicate<T>::value, list<T>, list<>>,
			                                       typename filter_impl<Predicate, Rest...>::type>::type;
		};

		template <template <typename> class Predicate, typename... Ts>
		auto filter_helper_list(list<Ts...>) -> typename filter_impl<Predicate, Ts...>::type;
	} // namespace detail

	template <template <typename> class Predicate, typename... Ts>
	using filter = typename detail::filter_impl<Predicate, Ts...>::type;

	template <template <typename> class Predicate, typename List>
	using filter_list = decltype(detail::filter_helper_list<Predicate>(std::declval<List>()));


	template <typename T, typename InT>
	T bit_cast(InT&& in)
	{
		static_assert(sizeof(InT) <= sizeof(T), "The result type has to be at least as large as the input.");
		static_assert(std::is_standard_layout_v<T>, "The result type has to be a standard layout type.");
		static_assert(std::is_standard_layout_v<InT>, "The input type has to be a standard layout type.");
		static_assert(std::is_trivially_copyable_v<T>, "The result type has to be trivial.");

		auto r = T();
		std::memcpy(&r, &in, sizeof(T));
		return r;
	}

	template <typename T>
	T bit_cast(const char* addr)
	{
		static_assert(std::is_standard_layout_v<T>, "The result type has to be a standard layout type.");
		static_assert(std::is_trivially_copyable_v<T>, "The result type has to be trivial.");

		auto r = T();
		std::memcpy(&r, addr, sizeof(T));
		return r;
	}


	template <typename T, typename K>
	void erase_fast(std::vector<T>& c, const K& v)
	{
		using std::swap;

		auto e = std::find(c.begin(), c.end(), v);
		if(e != c.end()) {
			swap(*e, c.back());
			c.pop_back();
		}
	}
	template <typename T, typename K>
	void erase_fast_stable(std::vector<T>& c, const K& v)
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

	template <typename T, typename F>
	auto map(std::vector<T>& c, F&& f)
	{
		auto result = std::vector<decltype(f(std::declval<T&>()))>();
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


	namespace detail {
		using type_id_t = std::intptr_t;

		template <typename T>
		struct type {
			static void id() {}
		};

		template <typename T>
		type_id_t type_id()
		{
			return reinterpret_cast<type_id_t>(&type<T>::id);
		}
	} // namespace detail


	template <typename T>
	class tracking_ptr;

	namespace detail {
		struct trackable_data {
			void*         obj_addr = nullptr;
			std::uint32_t revision = 0;

			trackable_data() = default;
			trackable_data(void* ptr, std::uint32_t rev) : obj_addr(ptr), revision(rev) {}
		};
	} // namespace detail

	template <typename T>
	class trackable {
	  public:
		trackable() = default;
		trackable(std::unique_ptr<T> obj) : _obj(std::move(obj)) {}
		trackable(trackable&&) = default;
		auto& operator         =(trackable&& rhs) noexcept
		{
			reset();

			_obj      = std::move(rhs._obj);
			_obj_addr = std::move(rhs._obj_addr);

			return *this;
		}
		~trackable() { reset(); }

		auto& operator=(std::unique_ptr<T> obj)
		{
			_obj = std::move(obj);
			if(_obj_addr) {
				_obj_addr->obj_addr = _obj.get();
				_obj_addr->revision++;
			}

			return *this;
		}

		auto reset()
		{
			if(_obj_addr) {
				_obj_addr->obj_addr = nullptr;
				_obj_addr->revision++;
			}

			return std::move(_obj);
		}

		auto create_ptr() -> tracking_ptr<T>;

		auto get() -> T* { return _obj.get(); }
		auto get() const -> T* { return _obj.get(); }

		operator T*() { return get(); }
		operator const T*() const { return get(); }

		auto operator*() -> T&
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return *ptr;
		}
		auto operator*() const -> T&
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return *ptr;
		}

		auto operator-> () -> T*
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return ptr;
		}
		auto operator-> () const -> T*
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return ptr;
		}

	  private:
		template <typename>
		friend class tracking_ptr;

		std::unique_ptr<T>                      _obj;
		std::shared_ptr<detail::trackable_data> _obj_addr;

		auto _get_obj_addr()
		{
			if(!_obj_addr) {
				_obj_addr = std::make_shared<detail::trackable_data>(_obj.get(), 0);
			}

			return _obj_addr;
		}
	};

	template <typename T, typename... Args>
	auto make_trackable(Args&&... args)
	{
		return trackable<T>(std::make_unique<T>(std::forward<Args>(args)...));
	}

	template <typename T>
	class tracking_ptr {
	  public:
		tracking_ptr() = default;
		tracking_ptr(trackable<T>& t)
		  : _trackable(t._get_obj_addr()), _last_seen_revision(_trackable ? _trackable->revision : 0)
		{
		}

		tracking_ptr(const tracking_ptr<T>& t) = default;
		tracking_ptr(tracking_ptr<T>&& t)      = default;
		tracking_ptr& operator=(const tracking_ptr<T>& t) = default;
		tracking_ptr& operator=(tracking_ptr<T>&& t) = default;

		template <typename I, typename = std::enable_if_t<!std::is_same_v<I, T>>>
		explicit tracking_ptr(tracking_ptr<I> t)
		  : _trackable(t._trackable)
		  , _caster(+[](void* ptr) { return dynamic_cast<T*>(static_cast<I*>(ptr)); })
		  , _last_seen_revision(_trackable ? _trackable->revision : 0)
		{
			MIRRAGE_INVARIANT(t._caster == nullptr,
			                  "Casting tracking_ptrs can't be nested! Nice try though.");
		}

		auto modified(T* last_seen)
		{
			if(!_trackable) {
				return last_seen == nullptr ? false : true;
			}

			auto current_revision = _trackable->revision;
			if(_last_seen_revision != current_revision || last_seen == nullptr) {
				LOG(plog::debug) << "Modified " << _last_seen_revision << " != " << current_revision;
				_last_seen_revision = current_revision;
				return true;
			}

			return false;
		}

		auto get() -> T*
		{
			if(!_trackable)
				return nullptr;

			auto ptr = _trackable->obj_addr;

			if(_caster)
				return _caster(ptr);
			else
				return static_cast<T*>(ptr);
		}
		auto get() const -> const T* { return const_cast<tracking_ptr<T>*>(this)->get(); }

		operator T*() { return get(); }
		operator const T*() const { return get(); }

		explicit operator bool() const { return get() != nullptr; }
		auto     operator!() const { return get() != nullptr; }

		auto operator*() -> T&
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return *ptr;
		}
		auto operator*() const -> T&
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return *ptr;
		}

		auto operator-> () -> T*
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return ptr;
		}
		auto operator-> () const -> T*
		{
			auto ptr = get();
			MIRRAGE_INVARIANT(ptr, "Null-Pointer dereferenced!");
			return ptr;
		}

	  private:
		template <typename>
		friend class tracking_ptr;

		using Caster = T* (*) (void*);

		std::shared_ptr<detail::trackable_data> _trackable;
		Caster                                  _caster             = nullptr;
		std::uint32_t                           _last_seen_revision = 0;
	};

	template <typename T>
	auto trackable<T>::create_ptr() -> tracking_ptr<T>
	{
		return tracking_ptr<T>(*this);
	}


	class any_ptr {
	  public:
		any_ptr() : _ptr(nullptr), _type_id(0) {}
		any_ptr(std::nullptr_t) : _ptr(nullptr), _type_id(0) {}
		template <typename T>
		any_ptr(T* ptr) : _ptr(ptr), _type_id(detail::type_id<T>())
		{
		}

		any_ptr& operator=(std::nullptr_t)
		{
			_ptr     = nullptr;
			_type_id = 0;
			return *this;
		}
		template <typename T>
		any_ptr& operator=(T* ptr)
		{
			_ptr     = ptr;
			_type_id = detail::type_id<T>();
			return *this;
		}

		template <typename T>
		auto try_extract() const -> T*
		{
			if(detail::type_id<T>() == _type_id)
				return static_cast<T*>(_ptr);
			else
				return nullptr;
		}

	  private:
		void*             _ptr;
		detail::type_id_t _type_id;
	};

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


	namespace {
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
	} // namespace

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

		auto operator*() const { return std::apply(deref_join{}, _iters); }
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

	namespace {
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
	} // namespace

	template <class... Range>
	class join_range {
	  public:
		join_range() noexcept {}
		template <class... T>
		join_range(T&&... ranges) noexcept : _ranges(std::forward<T>(ranges)...), _size(min(ranges.size()...))
		{
		}

		auto begin() const noexcept { return std::apply(get_join_begin{}, _ranges); }
		auto begin() noexcept { return std::apply(get_join_begin{}, _ranges); }
		auto end() const noexcept
		{
			return std::apply(get_join_iterator{}, std::tuple_cat(std::make_tuple(_size), _ranges));
		}
		auto end() noexcept
		{
			return std::apply(get_join_iterator{}, std::tuple_cat(std::make_tuple(_size), _ranges));
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


	template <class Iter, class Type>
	class cast_iterator {
	  public:
		typedef typename Iter::iterator_category iterator_category;
		typedef Type                             value_type;
		typedef std::ptrdiff_t                   difference_type;
		typedef Type*                            pointer;
		typedef Type&                            reference;


		cast_iterator(Iter iter) : iter(iter) {}

		reference operator*() { return *reinterpret_cast<pointer>(*iter); }
		pointer   operator->() { return reinterpret_cast<pointer>(*iter); }

		cast_iterator& operator++()
		{
			++iter;
			return *this;
		}
		cast_iterator& operator--()
		{
			--iter;
			return *this;
		}

		cast_iterator operator++(int)
		{
			cast_iterator t = *this;
			++*this;
			return t;
		}

		cast_iterator operator--(int)
		{
			cast_iterator t = *this;
			--*this;
			return t;
		}

		bool operator==(const cast_iterator& o) const { return iter == o.iter; }
		bool operator!=(const cast_iterator& o) const { return iter != o.iter; }
		bool operator<(const cast_iterator& o) const { return iter < o.iter; }


	  private:
		Iter iter;
	};


	template <typename F>
	void doOnce(F f)
	{
		static bool first = true;
		if(first) {
			first = false;
			f();
		}
	}


	template <class CCTP, class ParentT>
	class Registered;

	template <class CCTP, class ChildT>
	class Registration {
	  public:
		Registration() = default;
		Registration(Registration&& rhs) noexcept : _children(std::move(rhs._children))
		{
			for(auto& c : _children) {
				c->_parent = this;
			}
		}
		Registration(const Registration& rhs) = delete;
		~Registration()
		{
			for(auto& c : _children) {
				c->_parent = nullptr;
			}
		}

		Registration& operator=(Registration&& rhs) noexcept
		{
			for(auto& c : _children) {
				c->_parent = nullptr;
			}

			_children = std::move(rhs._children);
			for(auto& c : _children) {
				c->_parent = this;
			}

			return *this;
		}
		Registration& operator=(const Registration& rhs) = delete;

	  protected:
		template <typename F>
		void foreach_child(F&& f)
		{
			for(auto c : _children) {
				f(*static_cast<ChildT*>(c));
			}
		}

	  private:
		friend class Registered<ChildT, CCTP>;

		std::vector<Registered<ChildT, CCTP>*> _children;

		static Registration* asRegistration(CCTP* self)
		{
			static_assert(std::is_base_of_v<Registration, CCTP>,
			              "The first template argument of Registration needs to be CRTP.");
			return static_cast<Registration*>(self);
		}
	};

	template <class CCTP, class ParentT>
	class Registered {
	  public:
		Registered() noexcept : _parent(nullptr) {}
		Registered(ParentT& p) noexcept : _parent(&p) { _registration()->_children.push_back(this); }
		Registered(Registered&& rhs) noexcept : _parent(rhs._parent)
		{
			rhs._parent = nullptr;

			if(_parent) {
				util::erase_fast(_registration()->_children, &rhs);
				_registration()->_children.push_back(this);
			}
		}
		Registered(const Registered& rhs) noexcept : _parent(rhs._parent)
		{
			if(_parent) {
				_registration()->_children.push_back(this);
			}
		}
		~Registered()
		{
			if(_parent) {
				// safe unless the casted this ptr is dereferenced
				util::erase_fast(_registration()->_children, this);
			}
		}

		Registered& operator=(Registered&& rhs) noexcept
		{
			if(_parent) {
				util::erase_fast(_registration()->_children, this);
			}

			_parent     = rhs._parent;
			rhs._parent = nullptr;
			if(_parent) {
				util::erase_fast(_registration()->_children, &rhs);
				_registration()->_children.push_back(this);
			}

			return *this;
		}
		Registered& operator=(const Registered& rhs) noexcept
		{
			if(_parent) {
				util::erase_fast(_registration()->_children, this);
			}

			_parent = rhs._parent;
			if(_parent) {
				_registration()->_children.push_back(this);
			}

			return *this;
		}

	  protected:
		auto parent() noexcept -> auto&
		{
			MIRRAGE_INVARIANT(_parent, "Deref nullptr");
			return *_parent;
		}

	  private:
		friend class Registration<ParentT, CCTP>;
		ParentT* _parent;

		auto _registration() { return Registration<ParentT, CCTP>::asRegistration(_parent); }
	};
} // namespace mirrage::util

#define M_REPEAT_1(X) X(0)
#define M_REPEAT_2(X) M_REPEAT_1(X) X(1)
#define M_REPEAT_3(X) M_REPEAT_2(X) X(2)
#define M_REPEAT_4(X) M_REPEAT_3(X) X(3)
#define M_REPEAT_5(X) M_REPEAT_4(X) X(4)
#define M_REPEAT_6(X) M_REPEAT_5(X) X(5)
#define M_REPEAT_7(X) M_REPEAT_6(X) X(6)
#define M_REPEAT_8(X) M_REPEAT_7(X) X(7)
#define M_REPEAT_9(X) M_REPEAT_8(X) X(8)
#define M_REPEAT_10(X) M_REPEAT_9(X) X(9)
#define M_REPEAT_11(X) M_REPEAT_10(X) X(10)
#define M_REPEAT_12(X) M_REPEAT_11(X) X(11)
#define M_REPEAT_13(X) M_REPEAT_12(X) X(12)
#define M_REPEAT_14(X) M_REPEAT_13(X) X(13)
#define M_REPEAT_15(X) M_REPEAT_14(X) X(14)
#define M_REPEAT_16(X) M_REPEAT_15(X) X(15)
#define M_REPEAT_17(X) M_REPEAT_16(X) X(16)

#define M_EXPAND(...) __VA_ARGS__

#define M_REPEAT_X(N, X) M_EXPAND(M_REPEAT_##N)(X)
#define M_REPEAT_(N, X) M_REPEAT_X(N, X)
#define M_REPEAT(N, X)            \
	do {                          \
		M_REPEAT_(M_EXPAND(N), X) \
	} while(false)
