#pragma once

#include <concurrentqueue.h>

#ifndef ECS_COMPONENT_INCLUDED
#include "component.hpp"
#endif

namespace mirrage::ecs {

	class Sparse_index_policy {
	  public:
		void attach(Entity_id, Component_index);
		void detach(Entity_id);
		void shrink_to_fit();
		auto find(Entity_id) const -> util::maybe<Component_index>;
		void clear();

	  private:
		std::unordered_map<Entity_id, Component_index> _table;
	};
	class Compact_index_policy {
	  public:
		Compact_index_policy();
		void attach(Entity_id, Component_index);
		void detach(Entity_id);
		void shrink_to_fit();
		auto find(Entity_id) const -> util::maybe<Component_index>;
		void clear();

	  private:
		std::vector<Component_index> _table;
	};


	template <class T>
	struct Pool_storage_policy_value_traits {
		static constexpr bool         supports_empty_values = true;
		static constexpr int_fast32_t max_free              = 8;
		using Marker_type                                   = Entity_handle;
		static constexpr Marker_type free_mark              = invalid_entity;

		static constexpr const Marker_type* marker_addr(const T* inst)
		{
			return T::component_base_t::marker_addr(inst);
		}
	};
	template <class T>
	constexpr bool Pool_storage_policy_value_traits<T>::supports_empty_values;
	template <class T>
	constexpr int_fast32_t Pool_storage_policy_value_traits<T>::max_free;
	template <class T>
	constexpr typename Pool_storage_policy_value_traits<T>::Marker_type
	        Pool_storage_policy_value_traits<T>::free_mark;


	template <std::size_t Chunk_size, class T>
	class Pool_storage_policy {
		using pool_t = util::pool<T, Chunk_size, Component_index, Pool_storage_policy_value_traits<T>>;

	  public:
		using iterator = typename pool_t::iterator;

		template <class... Args>
		auto emplace(Args&&... args) -> std::tuple<T&, Component_index>
		{
			return _pool.emplace_back(std::forward<Args>(args)...);
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

	  private:
		pool_t _pool;
	};



	template <class T>
	class Component_container : public Component_container_base {
		friend class Entity_manager;
		friend void load(sf2::JsonDeserializer& s, Entity_handle& e);
		friend void save(sf2::JsonSerializer& s, const Entity_handle& e);

	  public:
		Component_container(Entity_manager& m) : _manager(m)
		{
			T::_validate_type_helper();
			_index.clear();
		}

		auto value_type() const noexcept -> Component_type override { return component_type_id<T>(); }

	  protected:
		void restore(Entity_handle owner, Deserializer& deserializer) override
		{
			auto entity_id = get_entity_id(owner, _manager);
			if(entity_id == invalid_entity_id) {
				MIRRAGE_FAIL("emplace_or_find_now of component from invalid/deleted entity");
			}

			auto& comp = [&]() -> T& {
				auto comp_idx = _index.find(entity_id);
				if(comp_idx.is_some()) {
					return _storage.get(comp_idx.get_or_throw());
				}

				auto comp = _storage.emplace(_manager, owner);
				_index.attach(entity_id, std::get<1>(comp));
				return std::get<0>(comp);
			}();

			load_component(deserializer, comp);
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
				_index.shrink_to_fit();
				_storage.shrink_to_fit([&](auto, auto& comp, auto new_idx) {
					_index.attach(comp.owner_handle().id(), new_idx);
				});
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

						Insertion insertion;
						if(_queued_insertions.try_dequeue(insertion)) {
							auto entity_id = get_entity_id(std::get<1>(insertion), _manager);
							if(entity_id == invalid_entity_id) {
								_storage.erase(comp_idx, [&](auto, auto& comp, auto new_idx) {
									_index.attach(comp.owner_handle().id(), new_idx);
								});
							} else {
								_storage.replace(comp_idx, std::move(std::get<0>(insertion)));
								_index.attach(std::get<1>(insertion).id(), comp_idx);
							}
						} else {
							_storage.erase(comp_idx, [&](auto, auto& comp, auto new_idx) {
								auto entity_id = get_entity_id(comp.owner_handle(), _manager);
								_index.attach(entity_id, new_idx);
							});
							_unoptimized_deletes++;
						}
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

						auto comp = _storage.emplace(std::move(std::get<0>(insertions_buffer[i])));
						_index.attach(entity_id, std::get<1>(comp));
					}
				} else {
					break;
				}
			} while(true);
		}

	  public:
		template <typename F, typename... Args>
		void emplace(F&& init, Entity_handle owner, Args&&... args)
		{
			MIRRAGE_INVARIANT(owner != invalid_entity, "emplace on invalid entity");

			// construct T inplace inside the pair to avoid additional move
			auto inst = Insertion(std::piecewise_construct,
			                      std::forward_as_tuple(_manager, owner, std::forward<Args>(args)...),
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

		using iterator = typename T::storage_policy::iterator;

	  private:
		using Insertion = std::pair<T, Entity_handle>;

		template <class E>
		using Queue = moodycamel::ConcurrentQueue<E>;

		typename T::index_policy   _index;
		typename T::storage_policy _storage;

		Entity_manager&      _manager;
		Queue<Entity_handle> _queued_deletions;
		Queue<Insertion>     _queued_insertions;
		int                  _unoptimized_deletes = 0;
	};
} // namespace mirrage::ecs
