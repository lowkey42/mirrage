#pragma once

#ifndef MIRRAGE_ASSETMANAGER_INCLUDED
#include "asset_manager.hpp"
#endif

#include <async++.h>

namespace mirrage::asset {

	template <class R>
	auto Ptr<R>::get_blocking() const -> const R&
	{
		if(_cached_result)
			return *_cached_result;

		return *(_cached_result = &_task.get());
	}

	template <class R>
	auto Ptr<R>::get_if_ready() const -> util::maybe<R&>
	{
		if(_cached_result)
			return util::justPtr(_cached_result);

		return ready() ? util::nothing : get_blocking();
	}

	template <class R>
	auto Ptr<R>::ready() const -> bool
	{
		return _task.ready();
	}

	template <class R>
	void Ptr<R>::reset()
	{
		_aid           = {};
		_task          = {};
		_cached_result = nullptr;
	}


	namespace detail {
		template <class T>
		struct has_reload {
		  private:
			typedef char one;
			typedef long two;

			template <typename C>
			static one test(decltype(std::declval<Loader<C>>().reload(std::declval<istream>(),
			                                                          std::declval<C&>()))*);
			template <typename C>
			static two test(...);


		  public:
			enum { value = sizeof(test<T>(nullptr)) == sizeof(char) };
		};

		template <class T>
		constexpr auto has_reload_v = has_reload<T>::value;

		template <class TaskType, class T>
		constexpr auto is_task_v =
		        std::is_same_v<T,
		                       ::async::task<TaskType>> || std::is_same_v<T, ::async::shared_task<TaskType>>;

		template <typename T>
		auto Asset_container<T>::load(AID aid, const std::string& path, bool cache) -> Ptr<T>
		{
			auto lock = std::scoped_lock{_container_mutex};

			auto found = _assets.find(path);
			if(found != _assets.end())
				return {aid, found->second.task};

			// not found => load
			// clang-format off
			auto loading = async::spawn([path = std::string(path), aid, this] {
				return Loader<T>::load(_manager._open(aid, path));
			}).share();
			// clang-format on

			if(cache)
				_assets.try_emplace(path, Asset{aid, loading, _manager._last_modified(path)});

			return {aid, loading};
		}

		template <typename T>
		void Asset_container<T>::save(const AID& aid, const std::string& name, const T& obj)
		{
			auto lock = std::scoped_lock{_container_mutex};

			Loader<T>::save(_manager._open_rw(aid, name), obj);

			auto found = _assets.find(name);
			if(found != _assets.end() && &found.value().task.get() != &obj) {
				_reload_asset(found.value(), found.key()); // replace existing value
			}
		}

		template <typename T>
		void Asset_container<T>::shrink_to_fit() noexcept
		{
			auto lock = std::scoped_lock{_container_mutex};

			util::erase_if(_assets, [](const auto& v) { return v.second.task.refcount() <= 1; });
		}

		template <typename T>
		void Asset_container<T>::reload()
		{
			auto lock = std::scoped_lock{_container_mutex};

			for(auto&& entry : _assets) {
				auto last_mod = _manager._last_modified(entry.first);

				if(last_mod > entry.second.last_modified) {
					_reload_asset(const_cast<Asset&>(entry.second), entry.first);
				}
			}
		}

		// TODO: test if this actually works
		template <typename T>
		void Asset_container<T>::_reload_asset(Asset& asset, const std::string& path)
		{
			auto& old_value = const_cast<T&>(asset.task.get());

			asset.last_modified = _manager._last_modified(path);

			if constexpr(has_reload_v<T>) {
				Loader<T>::reload(_manager._open(asset.aid, path), old_value);
			} else {
				auto new_value = Loader<T>::load(_manager._open(asset.aid, path));

				if constexpr(std::is_same_v<decltype(new_value), async::task<T>>) {
					old_value = std::move(new_value.get());
				} else if constexpr(std::is_same_v<decltype(new_value), async::shared_task<T>>) {
					old_value = std::move(const_cast<T&>(new_value.get()));
				} else {
					old_value = std::move(new_value);
				}
			}

			// TODO: notify other systems about change
		}
	} // namespace detail


	template <typename T>
	auto Asset_manager::load(const AID& id, bool cache) -> Ptr<T>
	{
		auto a = load_maybe<T>(id, cache);
		if(a.is_nothing())
			throw std::system_error(Asset_error::resolve_failed, id.str());

		return a.get_or_throw();
	}

	template <typename T>
	auto Asset_manager::load_maybe(const AID& id, bool cache) -> util::maybe<Ptr<T>>
	{
		auto path = resolve(id);
		if(path.is_nothing())
			return util::nothing;

		return _find_container<T>().process([&](detail::Asset_container<T>& container) {
			return container.load(id, path.get_or_throw(), cache);
		});
	}

	template <typename T>
	void Asset_manager::save(const AID& id, const T& asset)
	{
		auto path = resolve(id, false);
		if(path.is_nothing())
			throw std::system_error(Asset_error::resolve_failed, id.str());

		auto container = _find_container<T>();
		if(container.is_nothing())
			throw std::system_error(Asset_error::stateful_loader_not_initialized, id.str());

		container.get_or_throw().save(id, path.get_or_throw(), asset);
	}

	template <typename T>
	void Asset_manager::save(const Ptr<T>& asset)
	{
		save(asset.aid(), *asset);
	}

	template <typename T, typename... Args>
	void Asset_manager::create_stateful_loader(Args&&... args)
	{
		auto key = util::type_uid_of<T>();

		auto lock = std::scoped_lock{_containers_mutex};

		auto container = _containers.find(key);
		if(container == _containers.end()) {
			_containers.emplace(
			        key, std::make_unique<detail::Asset_container<T>>(*this, std::forward<Args>(args)...));
		}
	}

	template <typename T>
	void Asset_manager::remove_stateful_loader()
	{
		auto lock = std::scoped_lock{_containers_mutex};

		_containers.erase(util::type_uid_of<T>());
	}

	template <typename T>
	auto Asset_manager::_find_container() -> util::maybe<detail::Asset_container<T>&>
	{
		auto key = util::type_uid_of<T>();

		auto lock = std::scoped_lock{_containers_mutex};

		auto container = _containers.find(key);
		if(container != _containers.end()) {
			return util::justPtr(static_cast<detail::Asset_container<T>*>(&*container->second));
		}

		// no container for T, yet
		if constexpr(std::is_default_constructible_v<Loader<T>>) {
			container = _containers.emplace(key, std::make_unique<detail::Asset_container<T>>(*this)).first;

			return util::justPtr(static_cast<detail::Asset_container<T>*>(&*container->second));
		} else {
			return util::nothing;
		}
	}

} // namespace mirrage::asset
