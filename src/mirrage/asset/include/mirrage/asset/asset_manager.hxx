#pragma once

#ifndef ASSETMANAGER_INCLUDED
#include "asset_manager.hpp"
#endif


namespace mirrage::asset {

	template <class T>
	void Asset_manager::_asset_reloader_impl(void* asset, istream in) {
		auto newAsset           = Loader<T>::load(std::move(in));
		*static_cast<T*>(asset) = std::move(*newAsset.get());
	}

	template <typename T>
	Ptr<T> Asset_manager::load(const AID& id, bool cache) {
		auto asset = load_maybe<T>(id, cache);

		if(asset.is_nothing())
			throw Loading_failed("asset not found: " + id.str());

		return asset.get_or_throw();
	}

	template <typename T>
	auto Asset_manager::load_maybe(const AID& id, bool cache, bool warn) -> util::maybe<Ptr<T>> {
		auto res = _assets.find(id);
		if(res != _assets.end())
			return Ptr<T>{*this, id, std::static_pointer_cast<const T>(res->second.data)};

		Location_type type;
		std::string   path;
		std::tie(type, path) = _locate(id, warn);

		auto asset = std::shared_ptr<T>{};

		switch(type) {
			case Location_type::none: return util::nothing;

			case Location_type::indirection:
				asset = Interceptor<T>::on_intercept(*this, std::move(path), id);
				break;

			case Location_type::file: {
				auto stream = _open(path, id);
				if(!stream)
					return util::nothing;

				asset = Loader<T>::load(std::move(stream.get_or_throw()));
				break;
			}
		}

		if(cache)
			_add_asset(id, path, &_asset_reloader_impl<T>, std::static_pointer_cast<void>(asset));

		return Ptr<T>{*this, id, asset};
	}

	template <typename T>
	void Asset_manager::save(const AID& id, const T& asset) {
		Loader<T>::store(_create(id), asset);
		_force_reload(id);
	}


	template <class R>
	Ptr<R>::Ptr() : _mgr(nullptr) {}

	template <class R>
	Ptr<R>::Ptr(Asset_manager& mgr, const AID& id, std::shared_ptr<const R> res)
	  : _mgr(&mgr), _ptr(res), _aid(id) {}

	template <class R>
	const R& Ptr<R>::operator*() {
		load();
		return *_ptr.get();
	}
	template <class R>
	const R& Ptr<R>::operator*() const {
		MIRRAGE_INVARIANT(*this, "Access to unloaded resource");
		return *_ptr.get();
	}

	template <class R>
	const R* Ptr<R>::operator->() {
		load();
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
	Ptr<R>::operator std::shared_ptr<const R>() {
		load();
		return _ptr;
	}

	template <class R>
	void Ptr<R>::load() {
		if(!_ptr) {
			MIRRAGE_INVARIANT(_mgr, "Tried to load unintialized resource-ref");
			MIRRAGE_INVARIANT(_aid, "Tried to load unnamed resource");
			*this = _mgr->load<R>(_aid);
		}
	}

	template <class R>
	bool Ptr<R>::try_load(bool cache, bool warn) {
		if(!_ptr) {
			MIRRAGE_INVARIANT(_mgr, "Tried to load unintialized resource-ref");
			MIRRAGE_INVARIANT(_aid, "Tried to load unnamed resource");
			auto loaded = _mgr->load_maybe<R>(_aid, cache, warn);
			if(loaded.is_some()) {
				*this = loaded.get_or_throw();
			}
		}

		return !!_ptr;
	}

	template <class R>
	void Ptr<R>::unload() {
		_ptr.reset();
	}
} // namespace mirrage::asset
