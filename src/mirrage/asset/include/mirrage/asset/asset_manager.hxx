#pragma once

#ifndef MIRRAGE_ASSETMANAGER_INCLUDED
#include "asset_manager.hpp"
#endif


namespace mirrage::asset {

	template <class R>
	Ptr<R>::Ptr(const AID& id, std::shared_ptr<R> res) : _aid(id), _ptr(res) {}

	template <class R>
	const R& Ptr<R>::operator*() {
		return *_ptr.get();
	}
	template <class R>
	const R& Ptr<R>::operator*() const {
		MIRRAGE_INVARIANT(*this, "Access to unloaded resource");
		return *_ptr.get();
	}

	template <class R>
	const R* Ptr<R>::operator->() {
		return _ptr.get();
	}
	template <class R>
	const R* Ptr<R>::operator->() const {
		MIRRAGE_INVARIANT(*this, "Access to unloaded resource");
		return _ptr.get();
	}

	template <class R>
	bool Ptr<R>::operator==(const Ptr& o) const noexcept {
		return _aid == o._aid;
	}
	template <class R>
	bool Ptr<R>::operator<(const Ptr& o) const noexcept {
		return _aid < o._aid;
	}

	template <class R>
	Ptr<R>::operator std::shared_ptr<const R>() const {
		return _ptr;
	}


	namespace detail {
		template <class T>
		struct has_reload {
		  private:
			typedef char one;
			typedef long two;

			template <typename C>
			static one test(decltype(std::declval<Loader<C>>().reload(std::declval<istream>(),
			                                                          std::declval<C&>())) *);
			template <typename C>
			static two test(...);


		  public:
			enum { value = sizeof(test<T>(nullptr)) == sizeof(char) };
		};

		template <class T>
		constexpr auto has_reload_v = has_reload<T>::value;

		template <class T>
		constexpr auto is_task_v =
		        std::is_same_v<
		                std::remove_reference_t<std::decay_t<T>>,
		                async::task> || std::is_same_v<std::remove_reference_t<std::decay_t<T>>, async::shared_task>;

		template <typename T>
		auto Asset_container<T>::load(AID aid, const std::string& path, bool cache) -> Loading<T> {
			auto lock = std::scoped_lock{_container_mutex};

			auto found = _assets.find(path);
			if(found != _assets.end())
				return found->second.ptr;

			// not found => load
			// clang-format off
			auto loading = async::spawn([path = std::string(path), aid, this] {
				auto result = this->load(_manager._open(aid, path));
				if constexpr(is_task_v<decltype(result)>) {
					// if the loader returned a task, warp it and return the task (unwrapping)
					return result.then([aid](auto&& r) {
						return Ptr<T>(aid, std::forward<decltype(r)>(r));
					});

				} else {
					return Ptr<T>(aid, std::move(result));
				};
			}).share();
			// clang-format on

			if(cache)
				_assets.try_emplace(path, Asset{loading, _manager._last_modified(path)});

			return loading;
		}

		template <typename T>
		void Asset_container<T>::save(const AID& aid, const std::string& name, const T& obj) {
			auto lock = std::scoped_lock{_container_mutex};

			this->save(_manager._open_rw(aid, name), obj);

			auto found = _assets.find(name);
			if(found != _assets.end() && &*found.value().ptr.get() != &obj) {
				_reload_asset(found.value(), found.key()); // replace existing value
			}
		}

		template <typename T>
		void Asset_container<T>::shrink_to_fit() noexcept {
			auto lock = std::scoped_lock{_container_mutex};

			util::erase_if(_assets, [](const auto& v) { return v.second.ptr.get()._ptr.use_count() <= 1; });
		}

		template <typename T>
		void Asset_container<T>::reload() {
			auto lock = std::scoped_lock{_container_mutex};

			for(auto iter = _assets.begin(); iter != _assets.end(); iter++) {
				auto&& key   = iter.key();
				auto&& value = iter.value();

				auto last_mod = _manager._last_modified(key);

				if(last_mod > value.last_modified) {
					_reload_asset(value, key);
				}
			}
		}

		template <typename T>
		void Asset_container<T>::_reload_asset(Asset& asset, const std::string& path) {
			auto& asset_ptr = asset.ptr.get();

			asset.last_modified = _manager._last_modified(path);

			if constexpr(has_reload_v<T>) {
				this->reload(_manager._open(asset_ptr.aid(), path), *asset_ptr);

			} else {
				static_assert(std::is_same_v<T&, decltype(*asset_ptr._ptr)>,
				              "The lhs should be an l-value reference!");

				static_assert(
				        std::is_same_v<T&&, decltype(std::move(*load(_manager._open(asset_ptr.aid(), path))))>,
				        "The rhs should be an r-value reference!");

				*asset_ptr._ptr = std::move(*load(_manager._open(asset_ptr.aid(), path)));
			}

			// TODO: notify other systems about change
		}
	} // namespace detail


	template <typename T>
	auto Asset_manager::load(const AID& id, bool cache) -> Loading<T> {
		auto a = load_maybe<T>(id, cache);
		if(a.is_nothing())
			throw std::system_error(Asset_error::resolve_failed);

		return a.get_or_throw();
	}

	template <typename T>
	auto Asset_manager::load_maybe(const AID& id, bool cache) -> util::maybe<Loading<T>> {
		auto path = resolve(id);
		if(path.is_nothing())
			return util::nothing;

		return _find_container<T>().process([&](detail::Asset_container<T>& container) {
			return container.load(id, path.get_or_throw(), cache);
		});
	}

	template <typename T>
	void Asset_manager::save(const AID& id, const T& asset) {
		auto path = resolve(id);
		if(path.is_nothing())
			throw std::system_error(Asset_error::resolve_failed);

		auto container = _find_container<T>();
		if(container.is_nothing())
			throw std::system_error(Asset_error::stateful_loader_not_initialized);

		container.get_or_throw().save(id, path.get_or_throw(), asset);
	}

	template <typename T>
	void Asset_manager::save(const Ptr<T>& asset) {
		save(asset.aid(), *asset);
	}

	template <typename T, typename... Args>
	void Asset_manager::create_stateful_loader(Args&&... args) {
		auto key = util::type_uid_of<T>();

		auto lock = std::scoped_lock{_containers_mutex};

		auto container = _containers.find(key);
		if(container == _containers.end()) {
			_containers.emplace(
			        key, std::make_unique<detail::Asset_container<T>>(*this, std::forward<Args>(args)...));
		}
	}

	template <typename T>
	void Asset_manager::remove_stateful_loader() {
		auto lock = std::scoped_lock{_containers_mutex};

		_containers.erase(util::type_uid_of<T>());
	}

	template <typename T>
	auto Asset_manager::_find_container() -> util::maybe<detail::Asset_container<T>&> {
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
