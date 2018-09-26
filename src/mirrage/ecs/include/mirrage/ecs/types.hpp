/** Basic types, e.g. handles for entities & components **********************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/ecs/entity_handle.hpp>

#include <mirrage/utils/atomic_utils.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/pool.hpp>

#include <concurrentqueue.h>

#include <cstdint>
#include <string>
#include <vector>


namespace mirrage::ecs {
	struct Deserializer;
	struct Serializer;

	class Entity_manager;
	class Entity_facet;

	using Component_index = int32_t;
	using Component_type  = int_fast16_t;

	namespace detail {
		extern Component_type id_generator();
	}

	template <typename t>
	auto component_type_id()
	{
		static const auto type_id = detail::id_generator();
		return type_id;
	}

	using Component_filter = std::function<bool(Component_type)>;


	/**
	 * Thread-safe facet to a single entity.
	 *
	 * All mutations (emplace, erase, erase_other) are deferred which causes the following limitations:
	 *   - get/has may not reflect the most recent state of the entity
	 *   - the behavious is undefined if a component is emplaced and erased during the same frame
	 *   - the order of operations is not guaranteed
	 *
	 * See ecs.hxx for implementation of template methods
	 */
	class Entity_facet {
	  public:
		Entity_facet() : _manager(nullptr), _owner(invalid_entity) {}

		template <typename T>
		util::maybe<T&> get();

		template <typename T>
		bool has();

		template <typename T, typename... Args>
		void emplace(Args&&... args);

		template <typename T, typename F, typename... Args>
		void emplace_init(F&& init, Args&&... args);

		template <typename T>
		void erase();

		template <typename... T>
		void erase_other();

		auto manager() noexcept -> Entity_manager& { return *_manager; }
		auto handle() const noexcept { return _owner; }

		auto valid() const noexcept -> bool;

		explicit operator bool() const noexcept { return valid(); }
		         operator Entity_handle() const noexcept { return handle(); }

		void reset() { _owner = invalid_entity; }

	  private:
		friend class Entity_manager;

		Entity_facet(Entity_manager& manager, Entity_handle owner);

		Entity_manager* _manager;
		Entity_handle   _owner;
	};
} // namespace mirrage::ecs
