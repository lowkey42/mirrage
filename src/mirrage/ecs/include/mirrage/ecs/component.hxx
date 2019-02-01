#pragma once

#include <concurrentqueue.h>

#ifndef MIRRAGE_ECS_COMPONENT_INCLUDED
#include "component.hpp"
#endif

namespace mirrage::ecs {

	class Sparse_index_policy {
		using Table = tsl::robin_map<Entity_id, Component_index>;

	  public:
		void attach(Entity_id, Component_index);
		void detach(Entity_id);
		void shrink_to_fit();
		auto find(Entity_id) const -> util::maybe<Component_index>;
		void clear();

		auto begin() const { return _table.begin(); }
		auto end() const { return _table.end(); }

		static constexpr bool sorted_iteration_supported = false;

		using iterator = Table::const_iterator;
		// sorted_begin()
		// sorted_end()

	  private:
		Table _table;
	};

	namespace detail {
		class Compact_index_policy_iterator {
		  public:
			using Value_iterator = std::vector<Component_index>::const_iterator;

			using iterator_category = std::input_iterator_tag;
			using value_type        = std::tuple<Entity_id, Component_index>;
			using difference_type   = std::int_fast32_t;
			using reference         = value_type;
			using pointer           = value_type*;

			Compact_index_policy_iterator() = default;
			Compact_index_policy_iterator(difference_type index, Value_iterator value)
			  : _index(index), _value(value)
			{
			}

			value_type  operator*() noexcept { return *get(); }
			value_type* operator->() noexcept { return get(); }
			value_type* get() noexcept
			{
				_last_value = std::tie(_index, *_value);
				return &_last_value;
			}

			auto operator+=(difference_type n) -> auto&
			{
				_index += n;
				_value += n;
				return *this;
			}
			auto operator-=(difference_type n) -> auto&
			{
				_index -= n;
				_value -= n;
				return *this;
			}

			auto operator[](difference_type i) -> value_type
			{
				auto x = *this;
				x += i;
				return *x;
			}

			auto operator++() -> auto&
			{
				*this += 1;
				return *this;
			}
			auto operator++(int)
			{
				auto self = *this;
				++(*this);
				return self;
			}
			auto operator--() -> auto&
			{
				*this -= 1;
				return *this;
			}

			auto operator--(int)
			{
				auto self = *this;
				--(*this);
				return self;
			}

			friend auto operator-(const Compact_index_policy_iterator& lhs,
			                      const Compact_index_policy_iterator& rhs) noexcept
			{
				return lhs._index - rhs._index;
			}
			friend auto operator+(Compact_index_policy_iterator iter, difference_type offset)
			{
				iter += offset;
				return iter;
			}
			friend auto operator+(difference_type offset, Compact_index_policy_iterator iter)
			{
				iter += offset;
				return iter;
			}
			friend auto operator-(Compact_index_policy_iterator iter, difference_type offset)
			{
				iter -= offset;
				return iter;
			}

			friend auto operator<(const Compact_index_policy_iterator& lhs,
			                      const Compact_index_policy_iterator& rhs) noexcept
			{
				return lhs._index < rhs._index;
			}
			friend auto operator>(const Compact_index_policy_iterator& lhs,
			                      const Compact_index_policy_iterator& rhs) noexcept
			{
				return lhs._index > rhs._index;
			}
			friend auto operator<=(const Compact_index_policy_iterator& lhs,
			                       const Compact_index_policy_iterator& rhs) noexcept
			{
				return lhs._index <= rhs._index;
			}
			friend auto operator>=(const Compact_index_policy_iterator& lhs,
			                       const Compact_index_policy_iterator& rhs) noexcept
			{
				return lhs._index >= rhs._index;
			}
			friend auto operator==(const Compact_index_policy_iterator& lhs,
			                       const Compact_index_policy_iterator& rhs) noexcept
			{
				return lhs._index == rhs._index;
			}
			friend auto operator!=(const Compact_index_policy_iterator& lhs,
			                       const Compact_index_policy_iterator& rhs) noexcept
			{
				return !(lhs == rhs);
			}

		  private:
			difference_type _index = 0;
			Value_iterator  _value;
			value_type      _last_value;
		};
	} // namespace detail

	class Compact_index_policy {
	  public:
		using iterator = detail::Compact_index_policy_iterator;

		Compact_index_policy();
		void attach(Entity_id, Component_index);
		void detach(Entity_id);
		void shrink_to_fit();
		auto find(Entity_id) const -> util::maybe<Component_index>;
		void clear();

		static constexpr bool sorted_iteration_supported = true;

		auto sorted_begin() const -> iterator { return {0, _table.begin()}; }
		auto sorted_end() const -> iterator { return {std::int_fast32_t(_table.size()), _table.end()}; }

		auto begin() const { return sorted_begin(); }
		auto end() const { return sorted_end(); }

	  private:
		std::vector<Component_index> _table;
	};

	template <class Storage_policy>
	class Pool_based_index_policy {
	  public:
		Pool_based_index_policy(Storage_policy& pool) : _pool(&pool) {}
		void attach(Entity_id, Component_index) {}
		void detach(Entity_id) {}
		void shrink_to_fit() {}
		auto find(Entity_id entity) const -> util::maybe<Component_index> { return _pool->find(entity); }
		void clear() {}

		using iterator                                   = typename Storage_policy::iterator;
		static constexpr bool sorted_iteration_supported = false;
		// sorted_begin()
		// sorted_end()

		auto begin() { return _pool->begin(); }
		auto end() { return _pool->end(); }

	  private:
		Storage_policy* _pool;
	};


	namespace detail {
		template <class T, typename = void>
		struct Pool_storage_policy_sort {
			static constexpr bool sorted = false;
		};

		// FIXME: sorted pool returns/erases the wrong values under heavy contention
		template <class T>
		struct Pool_storage_policy_sort<T, util::void_t<decltype(T::sort_key), decltype(T::sort_key_index)>> {
			static constexpr bool sorted                   = true;
			static constexpr auto sort_key                 = T::sort_key();
			static constexpr auto sort_key_constructor_idx = T::sort_key_index;
		};

		template <class T, std::size_t Holes>
		struct Pool_storage_policy_value_traits : Pool_storage_policy_sort<T> {
			static constexpr int_fast32_t max_free = Holes;
		};
	} // namespace detail



	template <std::size_t Chunk_size, std::size_t Holes, class T>
	class Pool_storage_policy {
		using pool_t =
		        util::pool<T, Chunk_size, detail::Pool_storage_policy_value_traits<T, Holes>, Component_index>;

	  public:
		static constexpr auto is_sorted = pool_t::sorted;

		using iterator = typename pool_t::iterator;

		template <typename F, class... Args>
		auto emplace(F&& relocate, Args&&... args) -> std::tuple<T&, Component_index>
		{
			return _pool.emplace(relocate, std::forward<Args>(args)...);
		}

		void replace(Component_index idx, T&& new_element) { _pool.replace(idx, std::move(new_element)); }

		template <typename F>
		void erase(Component_index idx, F&& relocate)
		{
			_pool.erase(idx, std::forward<F>(relocate));
		}

		void clear() { _pool.clear(); }

		template <typename F>
		void shrink_to_fit(F&& relocate)
		{
			_pool.shrink_to_fit(std::forward<F>(relocate));
		}

		auto get(Component_index idx) -> T& { return _pool.get(idx); }

		auto begin() noexcept -> iterator { return _pool.begin(); }
		auto end() noexcept -> iterator { return _pool.end(); }
		auto size() const -> Component_index { return _pool.size(); }
		auto empty() const -> bool { return _pool.empty(); }

		template <class Key,
		          class = std::enable_if_t<std::is_same_v<Key, decltype(std::declval<T>().*T::sort_key())>>>
		auto find(const Key& key)
		{
			return _pool.find(key);
		}

	  private:
		pool_t _pool;
	};


	template <class T>
	class Void_storage_policy {
	  public:
		static constexpr auto is_sorted = false;

		/// dummy returned by all calls. Can be mutable because it doesn't contain any state
		static T dummy_instance;

		using iterator = T*;

		template <typename F, class... Args>
		auto emplace(F&&, Args&&...) -> std::tuple<T&, Component_index>
		{
			_size++;
			return {dummy_instance, 0};
		}

		void replace(Component_index, T&&) {}

		template <typename F>
		void erase(Component_index, F&&)
		{
			_size--;
		}

		void clear() { _size = 0; }

		template <typename F>
		void shrink_to_fit(F&&)
		{
		}

		auto get(Component_index) -> T& { return dummy_instance; }

		auto begin() noexcept -> iterator
		{
			static_assert(util::dependent_false<T>(), "Iteration is not supported by Void_storage_policy.");
			return &dummy_instance;
		}
		auto end() noexcept -> iterator
		{
			static_assert(util::dependent_false<T>(), "Iteration is not supported by Void_storage_policy.");
			return &dummy_instance + 1;
		}
		auto size() const -> Component_index { return _size; }
		auto empty() const -> bool { return _size == 0; }

	  private:
		std::size_t _size = 0;
	};

	template <class T>
	T Void_storage_policy<T>::dummy_instance;


	namespace detail {
		template <class Index, class Storage>
		auto index_constructor_arg(Storage& storage)
		{
			if constexpr(std::is_constructible_v<Index, Storage&>) {
				return Index(storage);
			} else {
				return Index();
			}
		}
	} // namespace detail

	template <class T>
	class Component_container : public Component_container_base {
		friend class Entity_manager;
		friend void load(sf2::JsonDeserializer& s, Entity_handle& e);
		friend void save(sf2::JsonSerializer& s, const Entity_handle& e);

	  public:
		Component_container(Entity_manager& m)
		  : _storage(), _index(detail::index_constructor_arg<typename T::index_policy>(_storage)), _manager(m)
		{
			T::_check_type_invariants();
			_index.clear();
		}

		auto value_type() const noexcept -> Component_type override { return component_type_id<T>(); }

	  protected:
		void restore(Entity_handle owner, Deserializer& deserializer) override
		{
			if constexpr(std::is_constructible_v<T, Entity_handle, Entity_manager&>) {
				auto entity_id = get_entity_id(owner, _manager);
				if(entity_id == invalid_entity_id) {
					MIRRAGE_FAIL("emplace_or_find_now of component from invalid/deleted entity");
				}

				auto& comp = [&]() -> T& {
					auto comp_idx = _index.find(entity_id);
					if(comp_idx.is_some()) {
						return _storage.get(comp_idx.get_or_throw());
					}

					auto relocator = [&](auto, auto& comp, auto new_idx) {
						_index.attach(comp.owner_handle().id(), new_idx);
					};

					auto comp = _storage.emplace(relocator, owner, _manager);
					_index.attach(entity_id, std::get<1>(comp));
					return std::get<0>(comp);
				}();

				load_component(deserializer, comp);

			} else {
				(void) owner;
				(void) deserializer;
				MIRRAGE_FAIL("Tried to load component " << T::name()
				                                        << " that has no two-argument constructor!");
			}
		}

		bool save(Entity_handle owner, Serializer& serializer) override
		{
			return find(owner).process(false, [&](T& comp) {
				serializer.write_value(T::name_save_as());
				save_component(serializer, comp);
				return true;
			});
		}

		void clear() override
		{
			_queued_deletions  = Queue<Entity_handle>{}; // clear by moving a new queue into the old
			_queued_insertions = Queue<Insertion>{};     // clear by moving a new queue into the old
			_index.clear();
			_storage.clear();
			_unoptimized_deletes = 0;
			_index.shrink_to_fit();
			_storage.shrink_to_fit([&](auto, auto& comp, auto new_idx) {
				_index.attach(comp.owner_handle().id(), new_idx);
			});
		}

		void process_queued_actions() override
		{
			process_deletions();
			process_insertions();

			if(_unoptimized_deletes > 32) {
				_unoptimized_deletes = 0;
				_storage.shrink_to_fit([&](auto, auto& comp, auto new_idx) {
					_index.attach(comp.owner_handle().id(), new_idx);
				});
				_index.shrink_to_fit();
			}
		}

		void process_deletions()
		{
			std::array<Entity_handle, 16> deletions_buffer;

			do {
				std::size_t deletions =
				        _queued_deletions.try_dequeue_bulk(deletions_buffer.data(), deletions_buffer.size());
				if(deletions > 0) {
					for(auto i = 0ull; i < deletions; i++) {
						auto entity_id = get_entity_id(deletions_buffer[i], _manager);
						if(entity_id == invalid_entity_id) {
							LOG(plog::warning)
							        << "Discard delete of component " << T::name()
							        << " from invalid/deleted entity: " << entity_name(deletions_buffer[i]);
							continue;
						}

						auto comp_idx_mb = _index.find(entity_id);
						if(!comp_idx_mb) {
							continue;
						}

						auto comp_idx = comp_idx_mb.get_or_throw();
						_index.detach(entity_id);

						_storage.erase(comp_idx, [&](auto, auto& comp, auto new_idx) {
							auto entity_id = get_entity_id(comp.owner_handle(), _manager);
							_index.attach(entity_id, new_idx);
						});
						_unoptimized_deletes++;
					}
				} else {
					break;
				}
			} while(true);
		}
		void process_insertions()
		{
			std::array<Insertion, 8> insertions_buffer;

			do {
				std::size_t insertions = _queued_insertions.try_dequeue_bulk(insertions_buffer.data(),
				                                                             insertions_buffer.size());
				if(insertions > 0) {
					for(auto i = 0ull; i < insertions; i++) {
						auto entity_id = get_entity_id(std::get<1>(insertions_buffer[i]), _manager);
						if(entity_id == invalid_entity_id) {
							LOG(plog::warning)
							        << "Discard insertion of component from invalid/deleted entity: "
							        << entity_name(std::get<1>(insertions_buffer[i]));
							continue;
						}

						auto relocator = [&](auto, auto& comp, auto new_idx) {
							_index.attach(comp.owner_handle().id(), new_idx);
						};
						auto comp = _storage.emplace(relocator, std::move(std::get<0>(insertions_buffer[i])));
						_index.attach(entity_id, std::get<1>(comp));
					}
				} else {
					break;
				}
			} while(true);
		}

	  public:
		using iterator       = typename T::storage_policy::iterator;
		using component_type = T;

		template <typename F, typename... Args>
		void emplace(F&& init, Entity_handle owner, Args&&... args)
		{
			MIRRAGE_INVARIANT(owner != invalid_entity, "emplace on invalid entity");

			// construct T inplace inside the pair to avoid additional move
			auto inst = Insertion(std::piecewise_construct,
			                      std::forward_as_tuple(owner, _manager, std::forward<Args>(args)...),
			                      std::forward_as_tuple(owner));
			std::forward<F>(init)(inst.first);
			_queued_insertions.enqueue(std::move(inst));
		}

		void erase(Entity_handle owner) override
		{
			MIRRAGE_INVARIANT(owner, "erase on invalid entity");
			_queued_deletions.enqueue(owner);
		}

		auto find(Entity_handle owner) -> util::maybe<T&>
		{
			auto entity_id = get_entity_id(owner, _manager);

			return _index.find(entity_id).process(
			        util::maybe<T&>(), [&](auto comp_idx) { return util::justPtr(&_storage.get(comp_idx)); });
		}
		auto has(Entity_handle owner) const -> bool
		{
			auto entity_id = get_entity_id(owner, _manager);

			return _index.find(entity_id).is_some();
		}

		auto begin() noexcept { return _storage.begin(); }
		auto end() noexcept { return _storage.end(); }
		auto size() const noexcept { return _storage.size(); }
		auto empty() const noexcept { return _storage.empty(); }


		auto unsafe_find(Entity_id entity_id) -> util::maybe<T&>
		{
			return _index.find(entity_id).process(
			        util::maybe<T&>(), [&](auto comp_idx) { return util::justPtr(&_storage.get(comp_idx)); });
		}

		static constexpr auto sorted_iteration_supported =
		        T::storage_policy::is_sorted || T::index_policy::sorted_iteration_supported;

		template <class ComponentContainer>
		friend auto detail::container_begin(ComponentContainer&)
		        -> Sorted_component_iterator<typename ComponentContainer::component_type>;

		template <class ComponentContainer>
		friend auto detail::container_end(ComponentContainer&)
		        -> Sorted_component_iterator<typename ComponentContainer::component_type>;

		template <class>
		friend class Sorted_component_iterator;

	  private:
		using Insertion = std::pair<T, Entity_handle>;

		template <class E>
		using Queue = moodycamel::ConcurrentQueue<E>;

		typename T::storage_policy _storage;
		typename T::index_policy   _index;

		Entity_manager&      _manager;
		Queue<Entity_handle> _queued_deletions;
		Queue<Insertion>     _queued_insertions;
		int                  _unoptimized_deletes = 0;
	};


	template <class T>
	class Sorted_component_iterator {
	  public:
		static constexpr auto pool_based = T::storage_policy::is_sorted;
		using wrapped_iterator           = std::conditional_t<pool_based,
                                                    typename T::storage_policy::iterator,
                                                    typename T::index_policy::iterator>;

		using iterator_category = std::input_iterator_tag;
		using value_type        = std::tuple<Entity_id, T*>;
		using difference_type   = std::int_fast32_t;
		using reference         = value_type;
		using pointer           = value_type*;

		Sorted_component_iterator() : _container(nullptr), _iterator() {}
		Sorted_component_iterator(typename T::Pool* container, wrapped_iterator iterator)
		  : _container(container), _iterator(std::move(iterator))
		{
		}

		value_type  operator*() noexcept { return *get(); }
		value_type* operator->() noexcept { return get(); }
		value_type* get() noexcept
		{
			if constexpr(pool_based) {
				auto& component = *_iterator;
				_last_value     = value_type{(component.*T::sort_key()).id(), &component};
			} else {
				auto&& [entity, idx] = *_iterator;
				_last_value          = value_type{entity, &_container->_storage.get(idx)};
			}

			return &_last_value;
		}

		auto operator+=(difference_type n) -> auto&
		{
			_iterator += n;
			return *this;
		}
		auto operator-=(difference_type n) -> auto&
		{
			_iterator -= n;
			return *this;
		}

		auto operator[](difference_type i) -> value_type { return _iterator[i]; }

		auto operator++() -> auto&
		{
			++_iterator;
			return *this;
		}
		auto operator++(int)
		{
			auto self = *this;
			++(*this);
			return self;
		}
		auto operator--() -> auto&
		{
			--_iterator;
			return *this;
		}

		auto operator--(int)
		{
			auto self = *this;
			--(*this);
			return self;
		}

		friend auto operator-(const Sorted_component_iterator& lhs,
		                      const Sorted_component_iterator& rhs) noexcept
		{
			return lhs._iterator - rhs._iterator;
		}
		friend auto operator+(Sorted_component_iterator iter, difference_type offset)
		{
			iter += offset;
			return iter;
		}
		friend auto operator+(difference_type offset, Sorted_component_iterator iter)
		{
			iter += offset;
			return iter;
		}
		friend auto operator-(Sorted_component_iterator iter, difference_type offset)
		{
			iter -= offset;
			return iter;
		}

		friend auto operator<(const Sorted_component_iterator& lhs,
		                      const Sorted_component_iterator& rhs) noexcept
		{
			return lhs._iterator < rhs._iterator;
		}
		friend auto operator>(const Sorted_component_iterator& lhs,
		                      const Sorted_component_iterator& rhs) noexcept
		{
			return lhs._iterator > rhs._iterator;
		}
		friend auto operator<=(const Sorted_component_iterator& lhs,
		                       const Sorted_component_iterator& rhs) noexcept
		{
			return lhs._iterator <= rhs._iterator;
		}
		friend auto operator>=(const Sorted_component_iterator& lhs,
		                       const Sorted_component_iterator& rhs) noexcept
		{
			return lhs._iterator >= rhs._iterator;
		}
		friend auto operator==(const Sorted_component_iterator& lhs,
		                       const Sorted_component_iterator& rhs) noexcept
		{
			return lhs._iterator == rhs._iterator;
		}
		friend auto operator!=(const Sorted_component_iterator& lhs,
		                       const Sorted_component_iterator& rhs) noexcept
		{
			return !(lhs == rhs);
		}

	  private:
		typename T::Pool* _container;
		wrapped_iterator  _iterator;
		value_type        _last_value;
	};

	namespace detail {
		template <class ComponentContainer>
		auto container_begin(ComponentContainer& container)
		        -> Sorted_component_iterator<typename ComponentContainer::component_type>
		{

			if constexpr(ComponentContainer::component_type::storage_policy::is_sorted) {
				return Sorted_component_iterator<typename ComponentContainer::component_type>(
				        &container, container._storage.begin());

			} else if constexpr(ComponentContainer::component_type::index_policy::sorted_iteration_supported) {
				return Sorted_component_iterator<typename ComponentContainer::component_type>(
				        &container, container._index.sorted_begin());

			} else {
				return Sorted_component_iterator<typename ComponentContainer::component_type>(
				        &container, container._index.begin());
			}
		}

		template <class ComponentContainer>
		auto container_end(ComponentContainer& container)
		        -> Sorted_component_iterator<typename ComponentContainer::component_type>
		{
			if constexpr(ComponentContainer::component_type::storage_policy::is_sorted) {
				return Sorted_component_iterator<typename ComponentContainer::component_type>(
				        &container, container._storage.end());
			} else if constexpr(ComponentContainer::component_type::index_policy::sorted_iteration_supported) {
				return Sorted_component_iterator<typename ComponentContainer::component_type>(
				        &container, container._index.sorted_end());
			} else {
				return Sorted_component_iterator<typename ComponentContainer::component_type>(
				        &container, container._index.end());
			}
		}
	} // namespace detail

	template <class ComponentContainer, typename>
	auto sorted_begin(ComponentContainer& container)
	        -> Sorted_component_iterator<typename ComponentContainer::component_type>
	{
		static_assert(ComponentContainer::sorted_iteration_supported,
		              "Sorted iteration is not supported by this component type!");

		return detail::container_begin(container);
	}

	template <class ComponentContainer, typename>
	auto sorted_end(ComponentContainer& container)
	        -> Sorted_component_iterator<typename ComponentContainer::component_type>
	{
		static_assert(ComponentContainer::sorted_iteration_supported,
		              "Sorted iteration is not supported by this component type!");

		return detail::container_end(container);
	}

} // namespace mirrage::ecs
