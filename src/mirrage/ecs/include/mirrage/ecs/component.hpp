/** Basic types, e.g. handles for entities & components **********************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once
#define MIRRAGE_ECS_COMPONENT_INCLUDED

#include <mirrage/ecs/serializer.hpp>
#include <mirrage/ecs/types.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/pool.hpp>

#include <tsl/robin_map.h>

#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace mirrage::ecs {

	template <class T>
	class Component_container;


	struct Index_policy {
		void attach(Entity_id, Component_index); //< overrides previous assignments
		void detach(Entity_id);
		void shrink_to_fit();
		auto find(Entity_id) const -> util::maybe<Component_index>;
		void clear();

		// static constexpr bool sorted_iteration_supported
		// using iterator = iterator<tuple<Entity_id, Component_index>>
		// sorted_begin() -> iterator
		// sorted_end()   -> iterator
	};
	class Sparse_index_policy;  //< for rarely used components
	class Compact_index_policy; //< for frequently used components
	template <class Storage_policy>
	class Pool_based_index_policy; //< no additional storage, but O(log N) access and requires entity_handle in component

	template <class T>
	struct Storage_policy {
		static constexpr auto is_sorted = false;

		using iterator = void;
		auto begin() -> iterator;
		auto end() -> iterator;
		auto size() const -> Component_index;
		auto empty() const -> bool;

		template <typename F, class... Args>
		auto emplace(F&& relocate, Args&&... args) -> std::tuple<T&, Component_index>;
		void replace(Component_index, T&&);
		template <typename F>
		void erase(Component_index, F&& relocate);
		template <typename F>
		void shrink_to_fit(F&& relocate);
		auto get(Component_index) -> T&;
		void clear();
	};
	template <std::size_t Chunk_size, std::size_t Holes, class T>
	class Pool_storage_policy;
	template <class T>
	class Void_storage_policy; //< for empty components


	namespace detail {
		extern auto get_entity_facet(Entity_manager&, Entity_handle) -> util::maybe<Entity_facet>;

		/**
		 * Non-template base class for components that know their entity
		 */
		class Owned_component_base {
		  public:
			Owned_component_base() : _owner(invalid_entity) {}
			explicit Owned_component_base(Entity_handle owner) : _owner(owner) {}

			auto owner_handle() const noexcept -> Entity_handle
			{
				MIRRAGE_INVARIANT(_owner, "invalid component");
				return _owner;
			}
			auto owner(Entity_manager& manager) const -> util::maybe<Entity_facet>
			{
				return get_entity_facet(manager, owner_handle());
			}

		  protected:
			Entity_handle _owner;
		};
	} // namespace detail

	/**
	 * Base class for components.
	 * Should only be used directly by components that need to avoid the additional cost of an
	 *   Entity_handle and don't need to known their owner.
	 *
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
	template <class T,
	          class Index_policy   = Sparse_index_policy,
	          class Storage_policy = Pool_storage_policy<256, 32, T>>
	class Tiny_component {
	  public:
		template <class>
		friend class Component_container;

		using component_base_t = Tiny_component;
		using index_policy     = Index_policy;
		using storage_policy   = Storage_policy;
		using Pool             = Component_container<T>;
		// static constexpr auto name() {return "Component";}
		static constexpr const char* name_save_as() { return T::name(); }

		// required for pool storage policies. is_alive has to return true for every living instance
		//   and false if set_dead has been called.
		// the argument of set_dead is a pointer to a destroyed instance.
		//     static auto is_alive(const T*) -> bool
		//     static void set_dead(T*)

		Tiny_component()                 = default;
		Tiny_component(Tiny_component&&) = default;
		Tiny_component(Entity_handle, Entity_manager&) {}
		Tiny_component& operator=(Tiny_component&&) = default;

	  protected:
		~Tiny_component() = default;

	  private:
		static constexpr void _check_type_invariants()
		{
			static_assert(std::is_base_of<Tiny_component, T>::value, "");
			static_assert(std::is_move_assignable<T>::value, "");
			static_assert(std::is_move_constructible<T>::value, "");
		}
	};

	/**
	 * Base class of all components that know their own entity.
	 */
	template <class T,
	          class Index_policy   = Sparse_index_policy,
	          class Storage_policy = Pool_storage_policy<1024, 32, T>>
	class Component : public Tiny_component<T, Index_policy, Storage_policy>,
	                  public detail::Owned_component_base {
	  public:
		using component_base_t               = Component;
		static constexpr auto sort_key       = &Component::_owner;
		static constexpr auto sort_key_index = 0;

		Component() = default;
		Component(Entity_handle owner, Entity_manager&) : Owned_component_base(owner) {}
		Component(Component&&) = default;
		Component& operator=(Component&&) = default;

	  protected:
		~Component() = default;
	};

	/// Example usage:
	///   struct Player_tag : Stateless_tag_component<Player_tag, int,
	///                                               Sparse_index_policy,
	///                                               Pool_storage_policy<1024, 64, T>> {
	///       static constexpr auto name() { return "Player_tag"; }
	///   };
	template <class T,
	          class TagType,
	          class Index_policy   = Sparse_index_policy,
	          class Storage_policy = Pool_storage_policy<1024, 64, T>>
	class Tag_component : public Tiny_component<T, Sparse_index_policy, Pool_storage_policy<1024, 64, T>> {
	  public:
		using component_base_t = Tag_component;

		TagType value;

		friend void load_component(ecs::Deserializer& state, Tag_component& self)
		{
			state.read_value(self.value);
		}
		friend void save_component(ecs::Serializer& state, const Tag_component& self)
		{
			state.write_value(self.value);
		}

		Tag_component()                = default;
		Tag_component(Tag_component&&) = default;
		explicit Tag_component(Entity_handle, Entity_manager&, TagType value) : value(value) {}
		Tag_component& operator=(Tag_component&&) = default;

	  protected:
		~Tag_component() = default;
	};

	/// Example usage:
	///   struct Enemy_tag : Stateless_tag_component<Enemy_tag, Sparse_index_policy> {
	///       static constexpr auto name() { return "Enemy_tag"; }
	///   };
	template <class T, class Index_policy = Sparse_index_policy>
	class Stateless_tag_component : public Tiny_component<T, Index_policy, Void_storage_policy<T>> {
	  public:
		using component_base_t = Stateless_tag_component;

		friend void load_component(ecs::Deserializer& state, Stateless_tag_component&)
		{
			state.read_virtual();
		}
		friend void save_component(ecs::Serializer& state, const Stateless_tag_component&)
		{
			state.write_virtual();
		}

		Stateless_tag_component()                          = default;
		Stateless_tag_component(Stateless_tag_component&&) = default;
		Stateless_tag_component(Entity_handle, Entity_manager&) {}
		Stateless_tag_component& operator=(Stateless_tag_component&&) = default;

	  protected:
		~Stateless_tag_component() = default;
	};


	/**
	 * All operations puplic mutating operations are deferred
	 *   and inherently thread safe.
	 */
	class Component_container_base {
		friend class Entity_manager;
		friend void load(sf2::JsonDeserializer& s, Entity_handle& e);
		friend void save(sf2::JsonSerializer& s, const Entity_handle& e);

	  protected:
		Component_container_base()                                = default;
		Component_container_base(Component_container_base&&)      = delete;
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
		virtual auto value_type() const noexcept -> Component_type = 0;

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

		// static constexpr bool sorted_iteration_supported
	};

	/// Iterator into sorted range of components T as std::tuple<Entity_id, T*>
	template <class T>
	class Sorted_component_iterator;

	template <class T>
	using is_sorted_component = std::bool_constant<T::Pool::sorted_iteration_supported>;

	template <class T>
	constexpr auto is_sorted_component_v = is_sorted_component<T>::value;

	namespace detail {
		template <class ComponentContainer>
		auto container_begin(ComponentContainer&)
		        -> Sorted_component_iterator<typename ComponentContainer::component_type>;

		template <class ComponentContainer>
		auto container_end(ComponentContainer&)
		        -> Sorted_component_iterator<typename ComponentContainer::component_type>;
	} // namespace detail

	template <class ComponentContainer,
	          typename = std::enable_if_t<ComponentContainer::sorted_iteration_supported>>
	auto sorted_begin(ComponentContainer&)
	        -> Sorted_component_iterator<typename ComponentContainer::component_type>;

	template <class ComponentContainer,
	          typename = std::enable_if_t<ComponentContainer::sorted_iteration_supported>>
	auto sorted_end(ComponentContainer&)
	        -> Sorted_component_iterator<typename ComponentContainer::component_type>;

} // namespace mirrage::ecs

#include "component.hxx"
