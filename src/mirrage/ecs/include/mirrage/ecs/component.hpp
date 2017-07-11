/** Basic types, e.g. handles for entities & components **********************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once
#define ECS_COMPONENT_INCLUDED

#include <mirrage/ecs/types.hpp>
#include <mirrage/ecs/serializer.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/log.hpp>
#include <mirrage/utils/pool.hpp>

#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>


namespace lux {
namespace ecs {

	template<class T>
	class Component_container;


	struct Index_policy {
		void attach(Entity_id, Component_index); //< overrides previous assignments
		void detach(Entity_id);
		void shrink_to_fit();
		auto find(Entity_id)const -> util::maybe<Component_index>;
		void clear();
	};
	class Sparse_index_policy;  //< for rarely used components
	class Compact_index_policy; //< for frequently used components

	template<class T>
	struct Storage_policy {
		using iterator = void;
		auto begin() -> iterator;
		auto end() -> iterator;
		auto size()const -> Component_index;
		auto empty()const -> bool;

		template<class... Args>
		auto emplace(Args&&... args) -> std::tuple<T&, Component_index>;
		void replace(Component_index, T&&);
		template<typename F>
		void erase(Component_index, F&& relocate);
		template<typename F>
		void shrink_to_fit(F&& relocate);
		auto get(Component_index) -> T&;
		void clear();
	};
	template<std::size_t Chunk_size, class T>
	class Pool_storage_policy;


	/**
	 * Base class for components.
	 * All components have to be default-constructable, move-assignable and provide a storage_policy,
	 *   index_policy and a name and a constructor taking only User_data&, Entity_manager&, Entity_handle.
	 *
	 * A component is expected to be reasonable lightweight (i.e. cheap to move).
	 * Any component C may provide the following additional ADL functions for serialisation:
	 *  - void load_component(ecs::Deserializer& state, C& v)
	 *  - void save_component(ecs::Serializer& state, const C& v)
	 *
	 * The static constexpr methods name_save_as() may also be "overriden" replaced
	 *   in a component to implement more complex compontent behaviour (e.g. a Live- and a Storage-
	 *   Component that share the same name when storead but gets always loaded as a Storage-Component).
	 */
	template<class T, class Index_policy=Sparse_index_policy, class Storage_policy=Pool_storage_policy<32, T>>
	class Component {
		template<class>
		friend class Component_container;
		static constexpr void _validate_type_helper() {
			static_assert(std::is_base_of<component_base_t, T>::value, "");
			static_assert(std::is_default_constructible<T>::value, "");
			static_assert(std::is_move_assignable<T>::value, "");
			static_assert(std::is_move_constructible<T>::value, "");
		}

		public:
			static constexpr const Entity_handle* marker_addr(const Component* inst) {
				static_assert(std::is_standard_layout<Component>::value,
				              "standard layout is required for the pool storage policy");
				return reinterpret_cast<const Entity_handle*> (
				        reinterpret_cast<const char*>(inst) + offsetof(Component, _owner) );
			}

			using component_base_t = Component;
			using index_policy   = Index_policy;
			using storage_policy = Storage_policy;
			using Pool           = Component_container<T>;
			// static constexpr auto name() {return "Component";}
			static constexpr const char* name_save_as() {return T::name();}

			Component() : _manager(nullptr), _owner(invalid_entity) {}
			Component(Entity_manager& manager, Entity_handle owner)
			    : _manager(&manager), _owner(owner) {}
			Component(Component&&)noexcept = default;
			Component& operator=(Component&&) = default;

			auto owner_handle()const noexcept -> Entity_handle {
				INVARIANT(_owner, "invalid component");
				return _owner;
			}
			auto manager()const noexcept -> Entity_manager& {
				INVARIANT(_manager, "invalid component");
				return *_manager;
			}
			auto owner()const -> Entity_facet {
				return {manager(), owner_handle()};
			}

		protected:
			~Component()noexcept { //< protected destructor to avoid destruction by base-class
				_validate_type_helper();
			}

		private:
			Entity_manager* _manager;
			Entity_handle _owner;
	};


	/**
	 * All operations except for emplace_now, find_now and process_queued_actions are deferred
	 *   and inherently thread safe.
	 */
	class Component_container_base {
		friend class Entity_manager;
		friend void load(sf2::JsonDeserializer& s, Entity_handle& e);
		friend void save(sf2::JsonSerializer& s, const Entity_handle& e);

		protected:
			Component_container_base() = default;
			Component_container_base(Component_container_base&&) = delete;
			Component_container_base(const Component_container_base&) = delete;
			
			//< NOT thread-safe
			virtual void restore(Entity_handle owner, Deserializer&) = 0;

			//< NOT thread-safe; returns false if component doesn't exists
			virtual bool save(Entity_handle owner, Serializer&) = 0;

			//< NOT thread-safe
			virtual void process_queued_actions() = 0;

			/// NOT thread safe
			virtual void clear() = 0;

		public:
			/// thread safe
			virtual void erase(Entity_handle owner) = 0;
			
			///thread safe
			virtual auto value_type()const noexcept -> Component_type = 0;

			/// thread safe
			// auto find(Entity_handle owner) -> util::maybe<T&>
			
			/// thread safe
			// auto has(Entity_handle owner)const -> bool

			/// thread safe
			// void emplace(Entity_handle owner, Args&&... args);

			virtual ~Component_container_base() = default;

			// begin()
			// end()
			// size()
			// empty()
	};

} /* namespace ecs */
}

#include "component.hxx"
