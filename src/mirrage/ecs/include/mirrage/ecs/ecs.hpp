/** The core part of the entity-component-system *****************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#define ECS_INCLUDED

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/types.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/string_utils.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <moodycamel/concurrentqueue.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace mirrage::ecs {
	struct Serializer;


	// entity transfer object
	using ETO = std::string;

	/**
	 * The main functionality is thread-safe but the other methods require a lock to prevent
	 *   concurrent read access during their execution
	 */
	class Entity_manager : util::no_copy_move {
	  public:
		Entity_manager(asset::Asset_manager&, util::any_ptr userdata);

		// user interface; thread-safe
		auto emplace() noexcept -> Entity_facet;
		auto emplace(const std::string& blueprint) -> Entity_facet;
		auto get(Entity_handle entity) -> util::maybe<Entity_facet>;
		auto validate(Entity_handle entity) -> bool { return _handles.valid(entity); }

		// deferred to next call to process_queued_actions
		void erase(Entity_handle entity);

		template <typename C>
		auto list() -> Component_container<C>&;
		auto list(Component_type type) -> Component_container_base&;
		template <typename F>
		void list_all(F&& handler);

		auto& userdata() noexcept { return _userdata; }

		// serialization interface; not thread-safe (yet?)
		auto write_one(Entity_handle source) -> ETO;
		auto read_one(ETO data, Entity_handle target = invalid_entity) -> Entity_facet;

		void write(std::ostream&, Component_filter filter = {});
		void write(std::ostream&, const std::vector<Entity_handle>&, Component_filter filter = {});
		void read(std::istream&, bool clear = true, Component_filter filter = {});


		// manager/engine interface; not thread-safe
		void process_queued_actions();
		void clear();
		template <typename T>
		void register_component_type();
		auto component_type_by_name(const std::string& name) -> util::maybe<Component_type>;

	  private:
		friend class Entity_facet;
		friend class Entity_collection_facet;

		using Erase_queue = moodycamel::ConcurrentQueue<Entity_handle>;

		asset::Asset_manager& _assets;
		util::any_ptr         _userdata;

		Entity_handle_generator    _handles;
		Erase_queue                _queue_erase;
		std::vector<Entity_handle> _local_queue_erase;

		std::vector<std::unique_ptr<Component_container_base>> _components;
		std::unordered_map<std::string, Component_type>        _components_by_name;
	};


	class Entity_iterator {
	  public:
		typedef Entity_handle             value_type;
		typedef value_type&               reference;
		typedef value_type*               pointer;
		typedef std::forward_iterator_tag iterator_category;
		typedef int                       difference_type;

		Entity_iterator(const Entity_handle_generator& gen, Entity_handle handle) noexcept
		  : _gen(gen), _handle(handle) {}

		auto operator++(int) {
			_handle = _gen.next(_handle);
			return *this;
		}
		auto operator++() {
			auto i  = *this;
			_handle = _gen.next(_handle);
			return i;
		}
		reference operator*() noexcept { return _handle; }
		pointer   operator->() noexcept { return &_handle; }
		bool      operator==(const Entity_iterator& rhs) const noexcept { return _handle == rhs._handle; }
		bool      operator!=(const Entity_iterator& rhs) noexcept { return !(*this == rhs); }

	  private:
		const Entity_handle_generator& _gen;
		Entity_handle                  _handle;
	};

	class Entity_collection_facet {
	  public:
		typedef Entity_handle value_type;

		Entity_collection_facet(Entity_manager& manager);

		Entity_iterator begin() const;
		Entity_iterator end() const;

		void emplace_back(Entity_handle h) {
			MIRRAGE_INVARIANT(_manager.validate(h),
			                  "invalid entity in Entity_collection_facet.emplace_back()");
		}
		void clear();

	  private:
		Entity_manager& _manager;
	};
} // namespace mirrage::ecs

#include "ecs.hxx"
