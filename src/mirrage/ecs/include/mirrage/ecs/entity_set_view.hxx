#pragma once

#ifndef MIRRAGE_ENITY_SET_VIEW_INCLUDED
#include "entity_set_view.hpp"
#endif

namespace mirrage::ecs {

	namespace detail {

		template <class C, class Values>
		decltype(auto) get_value(Entity_manager& entities, Entity_id entity, Values& values)
		{
			(void) entities;
			(void) entity;
			(void) values;

			if constexpr(std::is_same_v<C, Entity_handle>) {
				return entities.get_handle(entity);
			} else if constexpr(std::is_same_v<C, Entity_facet>) {
				return entities.get(entities.get_handle(entity)).get_or_throw();

			} else {
				return *std::get<C*>(values);
			}
		}

		template <class SortedPools, class UnsortedPools, class C1, class... Cs>
		auto Entity_set_iterator<SortedPools, UnsortedPools, C1, Cs...>::get() noexcept -> value_type&
		{
			_last_value.emplace(get_value<C1>(_entities, _entity, _values),
			                    get_value<Cs>(_entities, _entity, _values)...);
			return _last_value.get_or_throw();
		}

		template <class... SortedPools, class... UnsortedPools>
		auto build_pool_mask(std::tuple<SortedPools*...>&   sorted_pools,
		                     std::tuple<UnsortedPools*...>& unsorted_pools)
		{
			auto sorted_sizes =
			        util::make_array<Component_index>(std::get<SortedPools*>(sorted_pools)->size()...);
			auto unsorted_sizes =
			        util::make_array<Component_index>(std::get<UnsortedPools*>(unsorted_pools)->size()...);

			auto min_sorted_size   = std::min_element(sorted_sizes.begin(), sorted_sizes.end());
			auto min_unsorted_size = std::min_element(unsorted_sizes.begin(), unsorted_sizes.end());

			if(min_unsorted_size != unsorted_sizes.end()
			   && (min_sorted_size == sorted_sizes.end() || *min_unsorted_size <= *min_sorted_size / 10)) {
				// no sorted pools or one of the unsorted pools is much smaller
				//  => iterate over smallest unsorted pool in random order and find other components by ID
				auto i        = 0;
				auto pool_idx = std::distance(unsorted_sizes.begin(), min_unsorted_size);
				return std::array<bool, sizeof...(SortedPools) + sizeof...(UnsortedPools)>{
				        (std::get<SortedPools*>(sorted_pools) && false)...,
				        ((std::get<UnsortedPools*>(unsorted_pools) && false) || i++ == pool_idx)...};

			} else {
				auto max_size = *min_sorted_size * 10;
				return std::array<bool, sizeof...(SortedPools) + sizeof...(UnsortedPools)>{
				        (std::get<SortedPools*>(sorted_pools)->size() <= max_size)...,
				        (std::get<UnsortedPools*>(unsorted_pools) && false)...};
			}
		}

		template <class... Cs>
		auto get_pools(Entity_manager& entities, const util::list<Cs...>&)
		        -> std::tuple<typename Cs::Pool*...>
		{
			return std::make_tuple(&entities.list<Cs>()...);
		}


		template <std::size_t I, class Mask, class... SortedPools, class... UnsortedPools>
		auto get_begin_impl_I(std::tuple<SortedPools*...>&   sorted_pools,
		                      std::tuple<UnsortedPools*...>& unsorted_pools,
		                      const Mask&                    mask)
		{
			if(mask[I]) {
				if constexpr(I < sizeof...(SortedPools))
					return detail::container_begin(*std::get<I>(sorted_pools));
				else
					return detail::container_begin(*std::get<I - sizeof...(SortedPools)>(unsorted_pools));
			} else {
				if constexpr(I < sizeof...(SortedPools))
					return decltype(detail::container_begin(*std::get<I>(sorted_pools))){};
				else
					return decltype(
					        detail::container_begin(*std::get<I - sizeof...(SortedPools)>(unsorted_pools))){};
			}
		}


		template <class Mask, class... SortedPools, class... UnsortedPools, std::size_t... I>
		auto get_begin_impl(std::tuple<SortedPools*...>&   sorted_pools,
		                    std::tuple<UnsortedPools*...>& unsorted_pools,
		                    const Mask&                    mask,
		                    std::index_sequence<I...>)
		        -> std::tuple<Sorted_component_iterator<
		                              typename std::remove_pointer_t<SortedPools>::component_type>...,
		                      Sorted_component_iterator<
		                              typename std::remove_pointer_t<UnsortedPools>::component_type>...>
		{
			return std::make_tuple(get_begin_impl_I<I>(sorted_pools, unsorted_pools, mask)...);
		}
		template <class Mask, class... SortedPools, class... UnsortedPools>
		auto get_begin(std::tuple<SortedPools*...>&   sorted_pools,
		               std::tuple<UnsortedPools*...>& unsorted_pools,
		               const Mask&                    mask)
		{
			return get_begin_impl(sorted_pools,
			                      unsorted_pools,
			                      mask,
			                      std::index_sequence_for<SortedPools..., UnsortedPools...>{});
		}

		template <std::size_t I, class Mask, class... SortedPools, class... UnsortedPools>
		auto get_end_impl_I(std::tuple<SortedPools*...>&   sorted_pools,
		                    std::tuple<UnsortedPools*...>& unsorted_pools,
		                    const Mask&                    mask)
		{
			if(mask[I]) {
				if constexpr(I < sizeof...(SortedPools))
					return detail::container_end(*std::get<I>(sorted_pools));
				else
					return detail::container_end(*std::get<I - sizeof...(SortedPools)>(unsorted_pools));
			} else {
				if constexpr(I < sizeof...(SortedPools))
					return decltype(detail::container_end(*std::get<I>(sorted_pools))){};
				else
					return decltype(
					        detail::container_end(*std::get<I - sizeof...(SortedPools)>(unsorted_pools))){};
			}
		}

		template <class Mask, class... SortedPools, class... UnsortedPools, std::size_t... I>
		auto get_end_impl(std::tuple<SortedPools*...>&   sorted_pools,
		                  std::tuple<UnsortedPools*...>& unsorted_pools,
		                  const Mask&                    mask,
		                  std::index_sequence<I...>)
		        -> std::tuple<Sorted_component_iterator<
		                              typename std::remove_pointer_t<SortedPools>::component_type>...,
		                      Sorted_component_iterator<
		                              typename std::remove_pointer_t<UnsortedPools>::component_type>...>
		{
			return std::make_tuple(get_end_impl_I<I>(sorted_pools, unsorted_pools, mask)...);
		}
		template <class Mask, class... SortedPools, class... UnsortedPools>
		auto get_end(std::tuple<SortedPools*...>&   sorted_pools,
		             std::tuple<UnsortedPools*...>& unsorted_pools,
		             const Mask&                    mask)
		{
			return get_end_impl(sorted_pools,
			                    unsorted_pools,
			                    mask,
			                    std::index_sequence_for<SortedPools..., UnsortedPools...>{});
		}


		template <class SortedPools, class UnsortedPools, class C1, class... Cs>
		Entity_set_iterator<SortedPools, UnsortedPools, C1, Cs...>::Entity_set_iterator(
		        Entity_manager& entities, SortedPools& sorted, UnsortedPools& unsorted, bool begin_iterator)
		  : _entities(entities)
		  , _iterator_mask(begin_iterator ? build_pool_mask(sorted, unsorted)
		                                  : decltype(build_pool_mask(sorted, unsorted)){false})
		  , _iterators(begin_iterator ? get_begin(sorted, unsorted, _iterator_mask)
		                              : decltype(get_begin(sorted, unsorted, _iterator_mask)){})
		  , _iterator_ends(begin_iterator ? get_end(sorted, unsorted, _iterator_mask)
		                                  : decltype(get_begin(sorted, unsorted, _iterator_mask)){})
		  , _sorted_pools(sorted)
		  , _unsorted_pools(unsorted)
		  , _entity(invalid_entity_id)
		{
			if(begin_iterator)
				_find_next_valid();
		}

		template <class SortedPools, class UnsortedPools, class C1, class... Cs>
		auto Entity_set_iterator<SortedPools, UnsortedPools, C1, Cs...>::operator++() -> Entity_set_iterator&
		{
			util::foreach_in_tuple(_iterators, [&](auto index, auto& iter) {
				constexpr auto I   = decltype(index)::value;
				auto&&         end = std::get<I>(_iterator_ends);

				if(iter != end)
					++iter;
			});

			_find_next_valid();
			return *this;
		}

		template <class SortedPools, class UnsortedPools, class C1, class... Cs>
		void Entity_set_iterator<SortedPools, UnsortedPools, C1, Cs...>::_find_next_valid()
		{
			auto entity = Entity_id(0);
			util::foreach_in_tuple(_iterators, [&](auto index, auto& iter) {
				constexpr auto I = decltype(index)::value;
				if(!_iterator_mask[I])
					return;

				auto&& end = std::get<I>(_iterator_ends);

				if(iter != end)
					entity = util::max(entity, std::get<Entity_id>(*iter));
			});

			auto success = true;
			do {
				success           = true;
				auto found_entity = entity;

				util::foreach_in_tuple(_iterators, [&](auto index, auto& iter) {
					constexpr auto I = decltype(index)::value;
					if(!success || !_iterator_mask[I])
						return;

					auto&& end = std::get<I>(_iterator_ends);

					while(iter != end && std::get<Entity_id>(*iter) < found_entity)
						++iter;

					if(iter != end) {
						auto&& value         = *iter;
						std::get<I>(_values) = std::get<1>(value);
						found_entity         = std::get<Entity_id>(value);
					} else {
						success = false;
					}
				});

				if(!success) {
					// reached end => set all iterators and give up
					util::foreach_in_tuple(_iterators, [&](auto index, auto& iter) {
						iter = std::get<decltype(index)::value>(_iterator_ends);
					});
					_entity = invalid_entity_id;
					return;
				}

				success = entity == found_entity;
				entity  = found_entity;


				if(success) {
					// find each unsorted component
					util::foreach_in_tuple(_unsorted_pools, [&](auto index, auto& pool) {
						constexpr auto I  = decltype(index)::value;
						constexpr auto GI = I + std::tuple_size_v<SortedPools>;

						if(success && !_iterator_mask[GI]) {
							auto value = pool->unsafe_find(entity);
							success    = value.is_some();

							if(success)
								std::get<GI>(_values) = &value.get_or_throw();
						}
					});

					util::foreach_in_tuple(_sorted_pools, [&](auto index, auto& pool) {
						constexpr auto I = decltype(index)::value;

						if(success && !_iterator_mask[I]) {
							auto value = pool->unsafe_find(entity);
							success    = value.is_some();

							if(value.is_some())
								std::get<I>(_values) = &value.get_or_throw();
						}
					});

					if(!success) {
						entity++;
					}
				}
			} while(!success);

			_entity = entity;
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
		return detail::Entity_set_iterator<decltype(_sorted_pools), decltype(_unsorted_pools), C1, Cs...>{
		        *_entities, _sorted_pools, _unsorted_pools, true};
	}

	template <class C1, class... Cs>
	auto Entity_set_view<C1, Cs...>::end()
	{
		return detail::Entity_set_iterator<decltype(_sorted_pools), decltype(_unsorted_pools), C1, Cs...>{
		        *_entities, _sorted_pools, _unsorted_pools, false};
	}


} // namespace mirrage::ecs
