/** Basic types, e.g. handles for entities & components **********************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#define MIRRAGE_ENITY_SET_VIEW_INCLUDED

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <algorithm>
#include <array>


namespace mirrage::ecs {

	namespace detail {
		template <class T>
		using is_unsorted_component = std::bool_constant<!T::Pool::sorted_iteration_supported>;

		template <class T>
		using has_pool_type = typename T::Pool;

		template <class... Cs>
		auto is_component_list_helper(util::list<Cs...>)
		        -> std::conjunction<util::is_detected<has_pool_type, Cs>...>;
		template <class Comp_list>
		using is_component_list = decltype(is_component_list_helper(std::declval<Comp_list>()));

		template <class Cs>
		using sorted_component_types = util::filter_list<is_sorted_component, Cs>;

		template <class Cs>
		using unsorted_component_types = util::filter_list<is_unsorted_component, Cs>;

		template <class... Cs>
		using pool_ptr_tuple = std::tuple<typename Cs::Pool*...>;

		template <class... Cs>
		auto pool_ptrs_helper(util::list<Cs...>) -> pool_ptr_tuple<Cs...>;
		template <class Component_list>
		using pool_ptrs = decltype(pool_ptrs_helper(std::declval<Component_list>()));

		template <class... Pools>
		auto pool_value_ptrs_helper(std::tuple<Pools*...>) -> std::tuple<typename Pools::component_type*...>;

		template <class Pool_tuple>
		using pool_value_ptrs = decltype(pool_value_ptrs_helper(std::declval<Pool_tuple>()));

		template <class... Pools>
		auto pool_iterator_helper(std::tuple<Pools*...>) -> std::tuple<
		        Sorted_component_iterator<typename std::remove_pointer_t<Pools>::component_type>...>;

		template <class PoolTuple>
		using pool_iterators = decltype(pool_iterator_helper(std::declval<PoolTuple>()));



		template <class SortedPools, class UnsortedPools, class C1, class... Cs>
		class Entity_set_iterator {
			static constexpr auto includes_entity =
			        std::is_same_v<C1, Entity_handle> || std::is_same_v<C1, Entity_facet>;

			using Pools =
			        decltype(std::tuple_cat(std::declval<SortedPools>(), std::declval<UnsortedPools>()));
			using Values         = pool_value_ptrs<Pools>;
			using Pool_iterators = pool_iterators<Pools>;

			static_assert((std::tuple_size_v<Pool_iterators>) > 0,
			              "At least one iterator has to be provided!");

		  public:
			using Sorted_pool_mask = std::array<bool, std::tuple_size_v<Pool_iterators>>;

			using iterator_category = std::input_iterator_tag;
			using value_type =
			        std::conditional_t<includes_entity, std::tuple<C1, Cs&...>, std::tuple<C1&, Cs&...>>;
			using difference_type = std::int_fast32_t;
			using reference       = value_type;

			Entity_set_iterator(Entity_manager& entities,
			                    SortedPools&    sorted,
			                    UnsortedPools&  unsorted,
			                    bool            begin_iterator);
			Entity_set_iterator(const Entity_set_iterator&) noexcept = default;

			auto& operator=(const Entity_set_iterator& rhs) noexcept
			{
				_iterators = rhs._iterators;
				_entity    = rhs._entity;
				_values    = rhs._values;
				return *this;
			}

			auto operator*() noexcept -> value_type& { return get(); }
			auto operator-> () noexcept -> value_type* { return &get(); }
			auto operator*() const noexcept -> const value_type& { return get(); }
			auto operator-> () const noexcept -> const value_type* { return &get(); }
			auto get() noexcept -> value_type&;
			auto get() const noexcept -> const value_type&;
			auto operator++() -> Entity_set_iterator&;
			auto operator++(int) -> Entity_set_iterator
			{
				auto self = *this;
				++*this;
				return self;
			}

			friend auto operator==(const Entity_set_iterator& lhs, const Entity_set_iterator& rhs) noexcept
			{
				return lhs._entity == rhs._entity;
			}
			friend auto operator!=(const Entity_set_iterator& lhs, const Entity_set_iterator& rhs) noexcept
			{
				return !(lhs == rhs);
			}

			/// skips approximatly the given number of elements
			void estimate_advance(std::size_t);

		  private:
			Entity_manager&        _entities;
			const Sorted_pool_mask _iterator_mask;
			Pool_iterators         _iterators;
			const Pool_iterators   _iterator_ends;
			SortedPools&           _sorted_pools;
			UnsortedPools&         _unsorted_pools;

			mutable util::maybe<value_type> _last_value;
			Entity_id                       _entity;
			Values                          _values;

			void _find_next_valid();
		};

	} // namespace detail


	/// Iteratable view of entites with a given set of components
	template <class C1, class... Cs>
	class Entity_set_view {
		static constexpr auto includes_entity =
		        std::is_same_v<C1, Entity_handle> || std::is_same_v<C1, Entity_facet>;
		using Components = std::conditional_t<includes_entity, util::list<Cs...>, util::list<C1, Cs...>>;
		using Sorted_components   = detail::sorted_component_types<Components>;
		using Unsorted_components = detail::unsorted_component_types<Components>;

		static_assert(detail::is_component_list<Components>::value,
		              "The type arguments of ecs::Entity_set_view need to be components!");

	  public:
		using Sorted_pools   = detail::pool_ptrs<Sorted_components>;
		using Unsorted_pools = detail::pool_ptrs<Unsorted_components>;


		Entity_set_view(Entity_manager&);

		auto begin();
		auto end();

		/// returns an upper bound for the size of this view
		auto estimate_size() const noexcept -> std::size_t
		{
			auto size = std::numeric_limits<std::size_t>::max();
			util::foreach_in_tuple(_sorted_pools, [&](auto, auto& pool) {
				size = std::min(size, static_cast<std::size_t>(pool->size()));
			});
			util::foreach_in_tuple(_unsorted_pools, [&](auto, auto& pool) {
				size = std::min(size, static_cast<std::size_t>(pool->size()));
			});

			return size;
		}

	  private:
		Entity_manager* _entities;
		Sorted_pools    _sorted_pools;
		Unsorted_pools  _unsorted_pools;
	};


	template <typename... Cs, typename>
	auto Entity_manager::list() -> Entity_set_view<Cs...>
	{
		return {*this};
	}

	/// for parallel iteration with async++
	template <typename Iter>
	class entity_set_partitioner_impl {
	  public:
		entity_set_partitioner_impl(Iter begin, Iter end, std::size_t range_size, std::size_t grain)
		  : _iter_begin(begin), _iter_end(end), _range_size(range_size), _grain(grain)
		{
		}

		Iter begin() const { return _iter_begin; }
		Iter end() const { return _iter_end; }
		auto split() -> entity_set_partitioner_impl
		{
			if(_range_size <= _grain)
				return {_iter_end, _iter_end, 0, _grain};

			auto new_begin = _iter_begin;
			_range_size /= 2;
			_iter_begin.estimate_advance(_range_size);

			return entity_set_partitioner_impl{new_begin, _iter_begin, _range_size, _grain};
		}

	  private:
		Iter        _iter_begin, _iter_end;
		std::size_t _range_size;
		std::size_t _grain;
	};

	template <typename ESV>
	auto entity_set_partitioner(ESV& esv, std::size_t pool_num_threads = async::hardware_concurrency())
	{
		const auto size  = esv.estimate_size();
		const auto grain = std::clamp(size / (4 * pool_num_threads), std::size_t(64), std::size_t(512));
		return entity_set_partitioner_impl(esv.begin(), esv.end(), size, grain);
	}

} // namespace mirrage::ecs

#include "entity_set_view.hxx"
