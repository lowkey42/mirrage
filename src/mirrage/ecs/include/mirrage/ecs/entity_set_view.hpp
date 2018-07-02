/** Basic types, e.g. handles for entities & components **********************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/ecs.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <algorithm>


namespace mirrage::ecs {

	namespace detail {
		template <class T>
		using is_unsorted_component = std::bool_constant<!T::Pool::sorted_iteration_supported>;


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
		auto pool_value_ptrs_helper(std::tuple<Pools...>) -> std::tuple<typename Pools::component_type*...>;

		template <class Pool_tuple>
		using pool_value_ptrs = decltype(pool_value_ptrs_helper(std::declval<Pool_tuple>()));


		template <class SortedPoolIterators, class UnsortedPools, class C1, class... Cs>
		class Entity_set_iterator {
			static_assert(std::tuple_size_v<SortedPoolIterators>> 0,
			              "At least one iterator has to be provided!");

			static constexpr auto includes_entity = std::is_same_v<C1, Entity_handle>;
			using Unsorted_values                 = pool_value_ptrs<UnsortedPools>;

		  public:
			using Access_order = std::array<std::size_t, std::tuple_size_v<SortedPoolIterators>>;

			using iterator_category = std::input_iterator_tag;
			using value_type =
			        std::conditional_t<includes_entity, std::tuple<C1, Cs&...>, std::tuple<C1&, Cs&...>>;
			using difference_type = std::int_fast32_t;
			using reference       = value_type;

			Entity_set_iterator(Entity_manager&       entities,
			                    SortedPoolIterators&& sorted_begin,
			                    SortedPoolIterators&& sorted_end,
			                    UnsortedPools&        unsorted,
			                    Access_order&&        order)
			  : _entities(&entities)
			  , _sorted(std::move(sorted_begin))
			  , _sorted_end(std::move(sorted_end))
			  , _unsorted_pools(&unsorted)
			  , _access_order(std::move(order))
			{
				_find_next_valid();
			}

			auto operator*() noexcept -> value_type& { return get(); }
			auto operator-> () noexcept -> value_type* { return &get(); }
			auto get() noexcept -> value_type&;
			auto operator++() -> Entity_set_iterator&;
			auto operator++(int) -> Entity_set_iterator
			{
				auto self = *this;
				++*this;
				return self;
			}

			friend auto operator==(const Entity_set_iterator& lhs, const Entity_set_iterator& rhs) noexcept
			{
				return std::get<0>(lhs._sorted) == std::get<0>(rhs._sorted);
			}
			friend auto operator!=(const Entity_set_iterator& lhs, const Entity_set_iterator& rhs) noexcept
			{
				return !(lhs == rhs);
			}

		  private:
			Entity_manager*         _entities;
			SortedPoolIterators     _sorted;
			SortedPoolIterators     _sorted_end;
			UnsortedPools*          _unsorted_pools;
			Access_order            _access_order;
			util::maybe<value_type> _last_value;
			Entity_handle           _entity;
			Unsorted_values         _unsorted_values;

			void _find_next_valid();
		};

	} // namespace detail


	/// Iteratable view of entites with a given set of components
	template <class C1, class... Cs>
	class Entity_set_view {
		static constexpr auto includes_entity = std::is_same_v<C1, Entity_handle>;
		using Components = std::conditional_t<includes_entity, util::list<Cs...>, util::list<C1, Cs...>>;
		using Sorted_components   = detail::sorted_component_types<Components>;
		using Unsorted_components = detail::unsorted_component_types<Components>;

	  public:
		using Sorted_pools   = detail::pool_ptrs<Sorted_components>;
		using Unsorted_pools = detail::pool_ptrs<Unsorted_components>;


		Entity_set_view(Entity_manager&);

		auto begin();
		auto end();

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

} // namespace mirrage::ecs

#define MIRRAGE_ECS_ENITY_SET_VIEW_INCLUDED
#include "entity_set_view.hxx"
