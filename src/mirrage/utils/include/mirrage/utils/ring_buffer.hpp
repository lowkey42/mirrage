/** a fixed capacity circular buffer *****************************************
 *                                                                           *
 * Copyright (c) 2017 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/maybe.hpp>

#include <vector>


namespace mirrage::util {

	template <class T>
	class ring_buffer {
	  public:
		ring_buffer(std::size_t capacity) : _data(capacity) {}
		template <typename Factory>
		ring_buffer(std::size_t capacity, Factory&& f) {
			_data.reserve(capacity);
			for(auto i = std::size_t(0); i < capacity; i++) {
				_data.emplace_back(f());
			}
		}

		auto peek() const noexcept {
			return _tail != _head ? util::justPtr(&_data.at(_tail)) : util::nothing;
		}
		auto peek() noexcept { return _tail != _head ? util::justPtr(&_data.at(_tail)) : util::nothing; }

		template <typename Consumer>
		void pop(Consumer&& c) noexcept {
			MIRRAGE_INVARIANT(_tail != _head, "ring_buffer underflow: " << _tail << " == " << _head);
			c(_data.at(_tail));
			_tail = (_tail + 1) % _data.size();
		}

		template <typename Consumer>
		auto pop_while(Consumer&& c) noexcept {
			while(!empty()) {
				if(!c(_data.at(_tail)))
					return false;

				_tail = (_tail + 1) % _data.size();
			}

			return true;
		}

		auto empty() const noexcept { return _tail == _head; }

		auto advance_head() {
			auto new_head = (_head + 1) % _data.size();
			if(new_head == _tail)
				return false;

			_head = new_head;
			return true;
		}
		auto head() -> auto& { return _data.at(_head); }

		auto capacity() const noexcept { return _data.size(); }


	  private:
		std::vector<T> _data;
		std::size_t    _head = 0;
		std::size_t    _tail = 0;
	};
} // namespace mirrage::util
