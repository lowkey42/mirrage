/** Handle (unique versioned id) of an entity + generator ********************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/atomic_utils.hpp>
#include <mirrage/utils/log.hpp>

#include <concurrentqueue.h>

#include <cstdint>
#include <string>
#include <vector>


namespace mirrage::ecs {

	class Entity_manager;

	using Entity_id = int32_t;
	class Entity_handle {
	  public:
		using packed_t                    = uint32_t;
		static constexpr uint8_t free_rev = 0b10000; // marks revisions as free,
		// is cut off when assigned to Entity_handle::revision

		constexpr Entity_handle() : _id(0), _revision(0) {}
		constexpr Entity_handle(Entity_id id, uint8_t revision) : _id(id), _revision(revision) {}

		constexpr explicit operator bool() const noexcept { return _id != 0; }

		constexpr Entity_id id() const noexcept { return _id; }
		constexpr void      id(Entity_id id) noexcept { _id = id; }

		constexpr uint8_t revision() const noexcept { return _revision; }
		constexpr void    revision(uint8_t revision) noexcept { _revision = revision; }
		void              increment_revision() noexcept { _revision++; }

		constexpr packed_t pack() const noexcept
		{
			return static_cast<packed_t>(_id) << 4 | static_cast<packed_t>(_revision);
		}
		static constexpr Entity_handle unpack(packed_t d) noexcept
		{
			return Entity_handle{static_cast<int32_t>(d >> 4), static_cast<uint8_t>(d & 0b1111)};
		}

	  private:
		int32_t _id : 28;
		uint8_t _revision : 4;
	};

	constexpr inline bool operator==(const Entity_handle& lhs, const Entity_handle& rhs) noexcept
	{
		return lhs.id() == rhs.id() && lhs.revision() == rhs.revision();
	}
	constexpr inline bool operator!=(const Entity_handle& lhs, const Entity_handle& rhs) noexcept
	{
		return lhs.id() != rhs.id() || lhs.revision() != rhs.revision();
	}
	inline bool operator<(const Entity_handle& lhs, const Entity_handle& rhs) noexcept
	{
		return std::make_tuple(lhs.id(), lhs.revision()) < std::make_tuple(rhs.id(), rhs.revision());
	}

	static_assert(sizeof(Entity_handle::packed_t) <= sizeof(void*),
	              "what the hell is wrong with your plattform?!");

	inline Entity_handle to_entity_handle(void* u)
	{
		return Entity_handle::unpack(
		        static_cast<Entity_handle::packed_t>(reinterpret_cast<std::intptr_t>(u)));
	}
	inline void* to_void_ptr(Entity_handle h)
	{
		return reinterpret_cast<void*>(static_cast<std::intptr_t>(h.pack()));
	}

	constexpr const auto invalid_entity    = Entity_handle{};
	constexpr const auto invalid_entity_id = invalid_entity.id();


	extern auto get_entity_id(Entity_handle h, Entity_manager&) -> Entity_id;
	extern auto entity_name(Entity_handle h) -> std::string;

	class Entity_handle_generator {
		using Freelist = moodycamel::ConcurrentQueue<Entity_handle>;

	  public:
		Entity_handle_generator(Entity_id max = 128) { _slots.resize(static_cast<std::size_t>(max)); }

		// thread-safe
		auto get_new() -> Entity_handle
		{
			Entity_handle h;
			if(_free.try_dequeue(h)) {
				auto& rev          = util::at(_slots, static_cast<std::size_t>(h.id() - 1));
				auto  expected_rev = static_cast<uint8_t>(h.revision() | Entity_handle::free_rev);
				h.revision(static_cast<uint8_t>(rev & ~Entity_handle::free_rev)); // mark as used

				bool success = rev.compare_exchange_strong(expected_rev, h.revision());
				MIRRAGE_INVARIANT(success, "My handle got stolen :(");

				return h;
			}

			auto slot = _next_free_slot++;

			return {slot + 1, 0};
		}

		// thread-safe
		auto get(Entity_id h) const noexcept -> Entity_handle
		{
			if(h == invalid_entity_id || h - 1 >= static_cast<Entity_id>(_slots.size()))
				return invalid_entity;

			return {h, util::at(_slots, static_cast<std::size_t>(h - 1))};
		}

		// thread-safe
		auto valid(Entity_handle h) const noexcept -> bool
		{
			return h
			       && (static_cast<Entity_id>(_slots.size()) <= h.id() - 1
			           || util::at(_slots, static_cast<std::size_t>(h.id() - 1)) == h.revision());
		}

		// NOT thread-safe
		auto free(Entity_handle h) -> Entity_handle
		{
			if(h.id() - 1 >= static_cast<Entity_id>(_slots.size())) {
				_slots.resize(static_cast<std::size_t>(h.id() - 1) * 2, 0);
			}

			auto rev    = util::at(_slots, static_cast<std::size_t>(h.id() - 1)).load();
			auto handle = Entity_handle{h.id(), rev};
			handle.increment_revision();

			// mark as free; no CAS required only written here and in get_new() if already in _free
			util::at(_slots, static_cast<std::size_t>(h.id() - 1))
			        .store(handle.revision() | Entity_handle::free_rev);


			_free.enqueue(handle);
			return {handle};
		}

		// NOT thread-safe
		auto next(Entity_handle curr = invalid_entity) const -> Entity_handle
		{
			// internal_id = external_id - 1, so we don't need to increment it here
			auto end = _next_free_slot.load();

			for(Entity_id id = curr.id(); id < end; id++) {
				if(id >= static_cast<Entity_id>(_slots.size())) {
					return Entity_handle{id + 1, 0};
				} else {
					auto rev = util::at(_slots, static_cast<std::size_t>(id)).load();
					if((rev & ~Entity_handle::free_rev) == rev) { // is marked used
						return Entity_handle{id + 1, rev};
					}
				}
			}

			return invalid_entity;
		}

		// NOT thread-safe
		void clear()
		{
			_slots.clear();
			_slots.resize(_slots.capacity(), 0);
			_next_free_slot = 0;
			_free           = Freelist{}; // clear by moving a new queue into the old
		}

	  private:
		util::vector_atomic<uint8_t> _slots;
		std::atomic<Entity_id>       _next_free_slot{0};
		Freelist                     _free;
	};
} // namespace mirrage::ecs
