#include <mirrage/utils/purgatory.hpp>

#include <memory>


namespace lux {
namespace util {

	purgatory::purgatory(std::size_t reserve) {
		_blocks.reserve(std::max(std::size_t(1), reserve*64/512));
		_entries.reserve(reserve);
		_blocks.emplace_back(std::make_unique<char[]>(block_size));
	}
	purgatory::purgatory(purgatory&& rhs)
	    : _blocks(std::move(rhs._blocks))
	    , _current_offset(rhs._current_offset)
	    , _entries(std::move(rhs._entries)) {

		rhs._current_offset = 0;
		rhs._entries.clear();
		rhs._blocks.resize(1); // delete all but the last first block
	}
	purgatory& purgatory::operator=(purgatory&& rhs) {
		if(&rhs==this)
			return *this;

		clear();
		_blocks = std::move(rhs._blocks);
		_current_offset = std::move(rhs._current_offset);
		_entries = std::move(rhs._entries);

		rhs._current_offset = 0;
		rhs._entries.clear();
		rhs._blocks.resize(1); // delete all but the last first block

		return *this;;
	}
	purgatory::~purgatory() {
		clear();
	}

	auto purgatory::_reserve(std::size_t size, std::size_t alignment) -> void* {
		auto begin      = static_cast<void*>(_blocks.back().get()+_current_offset);
		auto space_left = block_size-_current_offset;

		if(!std::align(alignment, size, begin, space_left)) {
			_blocks.emplace_back(std::make_unique<char[]>(block_size));
			_current_offset = 0;
			space_left = block_size;
			begin = _blocks.back().get();
			if(!std::align(alignment, size, begin, space_left)) {
				throw std::out_of_range{"Couldn't reserve memory in internal buffer"};
			}
		}

		_current_offset = static_cast<std::size_t>(reinterpret_cast<char*>(begin)+size - _blocks.back().get());

		return begin;
	}
	void purgatory::clear() {
		for(auto& e : _entries) {
			e.deleter(e.ptr);
		}

		_current_offset = 0;
		_entries.clear();
		_blocks.resize(1); // delete all but the last first block
	}

}
}
