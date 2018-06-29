/** Basic types, e.g. handles for entities & components **********************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once
#define ECS_COMPONENT_INCLUDED

#include <mirrage/ecs/serializer.hpp>
#include <mirrage/ecs/types.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/pool.hpp>

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
	};
	class Sparse_index_policy;      //< for rarely used components
	class Compact_index_policy;     //< for frequently used components
	class Sparse_void_index_policy; //< for empty components that are rarely used

	template <class T>
	struct Storage_policy {
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
	template <std::size_t Chunk_size, class T>
	class Pool_storage_policy;
	template <class T>
	class Void_storage_policy; //< for empty components


	namespace detail {
		/**
		 * Non-template base class for components that know their entity
		 */
		class Owned_component_base {
		  public:
			static auto is_alive(const Owned_component_base* self) -> bool
			{
				return util::bit_cast<Entity_handle>(reinterpret_cast<const char*>(self)
				                                     + offsetof(Owned_component_base, _owner))
				       != invalid_entity;
			}
			static void set_dead(Owned_component_base* self)
			{
				auto addr = reinterpret_cast<char*>(self) + offsetof(Owned_component_base, _owner);
				new(addr) Entity_handle(invalid_entity);
			}

			Owned_component_base() : _owner(invalid_entity) {}
			explicit Owned_component_base(Entity_handle owner) : _owner(owner) {}

			auto owner_handle() const noexcept -> Entity_handle
			{
				MIRRAGE_INVARIANT(_owner, "invalid component");
				return _owner;
			}
			auto owner(Entity_manager& manager) const -> Entity_facet { return {manager, owner_handle()}; }

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
	          class Storage_policy = Pool_storage_policy<32, T>>
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
	          class Storage_policy = Pool_storage_policy<32, T>>
	class Component : public Tiny_component<T, Index_policy, Storage_policy>,
	                  public detail::Owned_component_base {
	  public:
		using component_base_t               = Component;
		static constexpr auto sort_key       = &Component::_owner;
		static constexpr auto sort_key_index = 0;

		Component() = default;
		explicit Component(Entity_handle owner) : Owned_component_base(owner) {}
		Component(Component&&) = default;
		Component& operator=(Component&&) = default;

	  protected:
		~Component() = default;
	};

	template <class T, class TagType, TagType invalid_tag>
	class Tag_component : public Tiny_component<T, Sparse_index_policy, Pool_storage_policy<128, T>> {
	  public:
		using component_base_t = Tag_component;

		TagType value;

		// TODO: load/save

		static auto is_alive(const T* self) -> bool
		{
			return util::bit_cast<TagType>(reinterpret_cast<const char*>(self) + offsetof(T, value))
			       != invalid_tag;
		}
		static void set_dead(T* self)
		{
			auto addr = reinterpret_cast<char*>(self) + offsetof(T, value);
			new(addr) TagType(invalid_tag);
		}

		Tag_component()                = default;
		Tag_component(Tag_component&&) = default;
		explicit Tag_component(TagType value) : value(value) {}
		Tag_component& operator=(Tag_component&&) = default;

	  protected:
		~Tag_component() = default;
	};

	template <class T>
	class Stateless_tag_component
	  : public Tiny_component<T, Sparse_void_index_policy, Void_storage_policy<T>> {
	  public:
		using component_base_t = Stateless_tag_component;

		// TODO: load/save

		Stateless_tag_component()                          = default;
		Stateless_tag_component(Stateless_tag_component&&) = default;
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
	};
} // namespace mirrage::ecs

#include "component.hxx"
