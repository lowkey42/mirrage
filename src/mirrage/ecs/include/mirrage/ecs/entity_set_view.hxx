#pragma once

#ifndef MIRRAGE_ECS_ENITY_SET_VIEW_INCLUDED
#include "entity_set_view.hpp"
#endif

namespace mirrage::ecs {

	namespace detail {

		template <class C, class SortedPoolIterators, class Unsorted_values>
		decltype(auto) get_value(Entity_handle entity, SortedPoolIterators& sorted, Unsorted_values& unsorted)
		{
			(void) entity;
			(void) sorted;
			(void) unsorted;

			if constexpr(std::is_same_v<C, Entity_handle>)
				return entity;
			else if constexpr(is_sorted_component_v<C>)
				return *std::get<C*>(*std::get<Sorted_component_iterator<C>>(sorted));
			else
				return *std::get<C*>(unsorted);
		}

		template <class SortedPoolIterators, class UnsortedPools, class C1, class... Cs>
		auto Entity_set_iterator<SortedPoolIterators, UnsortedPools, C1, Cs...>::get() noexcept -> value_type&
		{
			_last_value.emplace(get_value<C1>(_entity, _sorted, _unsorted_values),
			                    get_value<Cs>(_entity, _sorted, _unsorted_values)...);
			return _last_value.get_or_throw();
		}

		template <std::size_t I = 0, class... Ts>
		void incr_nth(std::tuple<Ts...>& tuple, std::size_t i)
		{
			if(i == I)
				++std::get<I>(tuple);
			else {
				if constexpr(I + 1 < sizeof...(Ts))
					incr_nth<I + 1>(tuple, i);
				else
					MIRRAGE_FAIL("index is larger than tuple!");
			}
		}

		template <std::size_t I = 0, class... Ts>
		auto get_nth_entity(std::tuple<Ts...>& tuple, std::tuple<Ts...>& end_tuple, std::size_t i)
		        -> Entity_id
		{
			if(i == I) {
				auto&& iter = std::get<I>(tuple);
				auto&& end  = std::get<I>(end_tuple);

				return iter != end ? std::get<Entity_id>(*iter) : invalid_entity_id;

			} else {
				if constexpr(I + 1 < sizeof...(Ts))
					return get_nth_entity<I + 1>(tuple, end_tuple, i);
				else
					MIRRAGE_FAIL("index is larger than tuple!");
			}
		}

		template <std::size_t I = 0, class... Ts>
		auto incr_nth_until(std::tuple<Ts...>& tuple,
		                    std::tuple<Ts...>& end_tuple,
		                    std::size_t        i,
		                    Entity_id          entity) -> Entity_id
		{
			if(i == I) {
				auto&& iter = std::get<I>(tuple);
				auto&& end  = std::get<I>(end_tuple);

				while(iter != end && std::get<Entity_id>(*iter) < entity)
					++iter;

				return iter != end ? std::get<Entity_id>(*iter) : invalid_entity_id;

			} else {
				if constexpr(I + 1 < sizeof...(Ts))
					return incr_nth_until<I + 1>(tuple, end_tuple, i, entity);
				else
					MIRRAGE_FAIL("index is larger than tuple!");
			}
		}

		template <class SortedPoolIterators, class UnsortedPools, class C1, class... Cs>
		auto Entity_set_iterator<SortedPoolIterators, UnsortedPools, C1, Cs...>::operator++()
		        -> Entity_set_iterator&
		{
			incr_nth(_sorted, _access_order[0]);

			_find_next_valid();
			return *this;
		}

		template <class SortedPoolIterators, class UnsortedPools, class C1, class... Cs>
		void Entity_set_iterator<SortedPoolIterators, UnsortedPools, C1, Cs...>::_find_next_valid()
		{
			auto in_retry = false;
			auto entity   = invalid_entity_id;
			{
			retry:
				if(!in_retry) {
					entity = get_nth_entity(_sorted, _sorted_end, _access_order[0]);

				} else if(entity != invalid_entity_id) {
					entity = incr_nth_until(_sorted, _sorted_end, _access_order[0], entity);
				}
				in_retry = true;

				if(entity == invalid_entity_id) {
					// reached end => set all iterators and give up
					util::foreach_in_tuple(_sorted, [&](auto& iter) {
						iter = std::get<std::remove_reference_t<decltype(iter)>>(_sorted_end);
					});
					return;
				}

				// increment sorted pointers 1-N until >= first
				if constexpr((std::tuple_size_v<SortedPoolIterators>) > 1) {
					for(auto idx : util::range(_access_order.begin() + 1, _access_order.end())) {
						auto found_entity = incr_nth_until(_sorted, _sorted_end, idx, entity);
						if(entity != found_entity) {
							entity = found_entity;
							goto retry;
						}
					}
				}

				auto entity_id          = _entities->get_handle(entity);
				auto all_unsorted_found = true;

				// find each unsorted component
				util::foreach_in_tuple(*_unsorted_pools, [&](auto& pool) {
					if(all_unsorted_found) {
						auto&& value = pool->get(entity_id);
						std::get<std::remove_reference_t<decltype(value)>*>(_unsorted_values) = value;
						all_unsorted_found &= value;
					}
				});

				if(!all_unsorted_found) {
					entity++;
					goto retry;
				}
			}
		}

		template <class... Cs>
		auto get_pools(Entity_manager& entities, const util::list<Cs...>&)
		        -> std::tuple<typename Cs::Pool*...>
		{
			return std::make_tuple(&entities.list<Cs>()...);
		}

		template <class... Pools>
		auto get_begin(std::tuple<Pools*...>& pools) -> std::tuple<
		        Sorted_component_iterator<typename std::remove_pointer_t<Pools>::component_type>...>
		{
			return std::make_tuple(sorted_begin(*std::get<Pools*>(pools))...);
		}

		template <class... Pools>
		auto get_end(std::tuple<Pools*...>& pools) -> std::tuple<
		        Sorted_component_iterator<typename std::remove_pointer_t<Pools>::component_type>...>
		{
			return std::make_tuple(sorted_end(*std::get<Pools*>(pools))...);
		}

		template <class... Pools>
		auto find_optimal_order(std::tuple<Pools*...>& pools)
		{
			auto order = std::array<std::size_t, sizeof...(Pools)>();
			std::iota(order.begin(), order.end(), 0);

			const auto sizes =
			        std::array<Component_index, sizeof...(Pools)>{std::get<Pools*>(pools)->size()...};

			std::sort(
			        order.begin(), order.end(), [&](auto lhs, auto rhs) { return sizes[lhs] < sizes[rhs]; });

			return order;
		}

	} // namespace detail

	template <class C1, class... Cs>
	Entity_set_view<C1, Cs...>::Entity_set_view(Entity_manager& entities)
	  : _entities(&entities)
	  , _sorted_pools(detail::get_pools(entities, detail::sorted_component_types<Components>{}))
	  , _unsorted_pools(detail::get_pools(entities, detail::unsorted_component_types<Components>{}))
	{
	}

	template <class C1, class... Cs>
	auto Entity_set_view<C1, Cs...>::begin()
	{
		// TODO[optimization/fix]: IF MIN(SIZE(unsorted_pool)) < N and MIN(SIZE(sorted_pool)) > M
		//							=> iterate over unsorted pool and treat sorted_pools as unsorted
		return detail::
		        Entity_set_iterator<decltype(get_begin(_sorted_pools)), decltype(_unsorted_pools), C1, Cs...>{
		                *_entities,
		                get_begin(_sorted_pools),
		                get_end(_sorted_pools),
		                _unsorted_pools,
		                find_optimal_order(_sorted_pools)};
	}

	template <class C1, class... Cs>
	auto Entity_set_view<C1, Cs...>::end()
	{
		// TODO[optimization/fix]: same as begin (for compatible type)
		return detail::
		        Entity_set_iterator<decltype(get_begin(_sorted_pools)), decltype(_unsorted_pools), C1, Cs...>{
		                *_entities,
		                get_end(_sorted_pools),
		                get_end(_sorted_pools),
		                _unsorted_pools,
		                std::array<std::size_t, std::tuple_size_v<Sorted_pools>>{}};
	}


} // namespace mirrage::ecs
