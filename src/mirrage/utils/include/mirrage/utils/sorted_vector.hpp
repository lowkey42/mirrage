/** a simple sorted container wrapper ****************************************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace mirrage::util {

	template <class T>
	class sorted_vector {
	  public:
		using const_iterator         = typename std::vector<T>::const_iterator;
		using const_reverse_iterator = typename std::vector<T>::const_reverse_iterator;

		sorted_vector() = default;
		sorted_vector(std::vector<T> data) : _data(data) { std::sort(data.begin(), data.end()); }

		template <class U, typename = std::enable_if_t<std::is_same_v<std::remove_reference_t<U>, T>>>
		auto insert(U&& v) -> T&
		{
			auto lb = std::lower_bound(_data.begin(), _data.end(), v);
			return *_data.insert(lb, std::forward<U>(v));
		}
		template <class Key,
		          class... Args,
		          typename = std::enable_if_t<!std::is_same_v<std::remove_reference_t<Key>, T>>>
		auto emplace(const Key& key, Args&&... args) -> T&
		{
			auto lb = std::lower_bound(_data.begin(), _data.end(), key);
			return *_data.emplace(lb, std::forward<Args>(args)...);
		}

		void erase(const T& v)
		{
			auto lb = std::lower_bound(_data.begin(), _data.end(), v);
			if(lb != _data.end() && *lb == v) {
				_data.erase(lb);
			}
		}

		template <class Key>
		auto find(const Key& key) -> const_iterator
		{
			auto iter = std::lower_bound(begin(), end(), key);
			return iter != end() && *iter == key ? iter : end();
		}

		void erase(const_iterator iter) { _data.erase(iter); }
		void erase(const_iterator begin, const_iterator end) { _data.erase(begin, end); }

		auto pop_back() -> T
		{
			auto v = back();
			_data.pop_back();
			return v;
		}

		auto front() { return _data.front(); }
		auto back() { return _data.back(); }

		auto clear() { return _data.clear(); }

		void reserve(std::size_t n) { _data.reserve(n); }

		auto size() const { return _data.size(); }
		auto empty() const { return _data.empty(); }
		auto begin() const { return _data.begin(); }
		auto end() const { return _data.end(); }
		auto rbegin() const { return _data.rbegin(); }
		auto rend() const { return _data.rend(); }

		decltype(auto) operator[](std::int64_t i) { return _data.at(i); }

	  private:
		std::vector<T> _data;
	};

} // namespace mirrage::util
