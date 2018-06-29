#include <mirrage/ecs/component.hpp>

namespace mirrage::ecs {

	void Sparse_index_policy::attach(Entity_id owner, Component_index comp)
	{
		if(owner != invalid_entity_id)
			_table[owner] = comp;
	}
	void Sparse_index_policy::detach(Entity_id owner)
	{
		if(owner != invalid_entity_id)
			_table.erase(owner);
	}
	void Sparse_index_policy::shrink_to_fit() {}
	auto Sparse_index_policy::find(Entity_id owner) const -> util::maybe<Component_index>
	{
		if(owner == invalid_entity_id)
			return util::nothing;

		auto iter = _table.find(owner);
		if(iter != _table.end()) {
			return iter->second;
		}

		return util::nothing;
	}
	void Sparse_index_policy::clear() { _table.clear(); }


	Compact_index_policy::Compact_index_policy() { _table.resize(32, -1); }

	void Compact_index_policy::attach(Entity_id owner, Component_index comp)
	{
		if(owner == invalid_entity_id)
			return;

		if(static_cast<Entity_id>(_table.size()) < owner) {
			auto capacity = static_cast<std::size_t>(std::max(owner, 64));
			_table.resize(capacity * 2, -1);
		}

		_table.at(static_cast<std::size_t>(owner)) = comp;
	}
	void Compact_index_policy::detach(Entity_id owner)
	{
		if(owner == invalid_entity_id)
			return;

		auto idx = static_cast<std::size_t>(owner);

		if(idx < _table.size()) {
			_table[idx] = -1;
		}
	}
	void Compact_index_policy::shrink_to_fit()
	{
		auto new_end = std::find_if(_table.rbegin(), _table.rend(), [](auto i) { return i != 0; });
		_table.erase(new_end.base(), _table.end());
		_table.shrink_to_fit();
	}
	auto Compact_index_policy::find(Entity_id owner) const -> util::maybe<Component_index>
	{
		if(owner == invalid_entity_id)
			return util::nothing;

		auto idx = static_cast<std::size_t>(owner);

		if(idx < _table.size()) {
			auto comp = _table[idx];
			if(comp >= 0) {
				return comp;
			}
		}

		return util::nothing;
	}
	void Compact_index_policy::clear() { _table.clear(); }


	void Sparse_void_index_policy::attach(Entity_id owner, Component_index) { _table.emplace(owner); }
	void Sparse_void_index_policy::detach(Entity_id owner) { _table.erase(owner); }
	void Sparse_void_index_policy::shrink_to_fit() {}
	auto Sparse_void_index_policy::find(Entity_id owner) const -> util::maybe<Component_index>
	{
		if(_table.count(owner) > 0)
			return Component_index(0);
		else
			return util::nothing;
	}
	void Sparse_void_index_policy::clear() { _table.clear(); }

} // namespace mirrage::ecs
