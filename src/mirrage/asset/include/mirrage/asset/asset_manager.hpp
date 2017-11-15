/** interface for loading & writing asset & files ****************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/aid.hpp>
#include <mirrage/asset/error.hpp>
#include <mirrage/asset/stream.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/reflection.hpp>
#include <mirrage/utils/stacktrace.hpp>
#include <mirrage/utils/string_utils.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <async++.h>
#include <tsl/robin_map.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>


/**
 * void example(asset::Manager& assetMgr) {
 *		asset::Loading<Texture> itemTex = assetMgr.load<Texture>("tex:items/health/small"_aid);
 *		Ptr<Texture> loaded = itemTex.get();
 * }
 */
namespace mirrage::asset {
	class Asset_manager;
	namespace detail {
		template <typename T>
		class Asset_container;
	}

	extern std::string pwd();

	extern void setup_storage();
	extern bool storage_ready();

	template <class R>
	class Ptr {
	  public:
		Ptr() = default;
		Ptr(const AID& id, std::shared_ptr<R> res = std::shared_ptr<R>());

		bool operator==(const Ptr& o) const noexcept;
		bool operator<(const Ptr& o) const noexcept;

		auto operator*() -> const R&;
		auto operator*() const -> const R&;

		auto operator-> () -> const R*;
		auto operator-> () const -> const R*;

		operator bool() const noexcept { return !!_ptr; }
		operator std::shared_ptr<const R>() const;

		auto aid() const noexcept -> const AID& { return _aid; }

		void reset() { _ptr.reset(); }

	  private:
		friend class detail::Asset_container<R>;

		AID                _aid;
		std::shared_ptr<R> _ptr;
	};

	template <typename T>
	using Loading = async::shared_task<Ptr<T>>;

	template <typename T>
	auto make_ready_asset(const AID& id, T&& val) -> Loading<T> {
		return async::make_task(Ptr<T>(id, std::make_shared<T>(std::move(val)))).share();
	}

	namespace detail {
		template <typename K, typename V>
		using Map =
		        tsl::robin_map<K, V, std::hash<K>, std::equal_to<>, std::allocator<std::pair<K, V>>, true>;


		class Asset_container_base {
		  public:
			virtual ~Asset_container_base() = default;

			virtual void shrink_to_fit() noexcept = 0;
			virtual void reload()                 = 0;
		};

		template <typename T>
		class Asset_container : public Asset_container_base, Loader<T> {
		  public:
			template <typename... Args>
			explicit Asset_container(Asset_manager& manager, Args&&... args)
			  : Loader<T>(std::forward<Args>(args)...), _manager(manager) {}

			using Loader<T>::load;
			using Loader<T>::save;

			auto load(const AID& aid, const std::string& name, bool cache) -> Loading<T>;

			void save(const AID& aid, const std::string& name, const T&);

			void shrink_to_fit() noexcept override;
			void reload() override;

		  private:
			struct Asset {
				Loading<T> ptr;
				int64_t    last_modified;
			};

			Asset_manager&          _manager;
			Map<std::string, Asset> _assets;
			std::mutex              _container_mutex;

			void _reload_asset(Asset&, const std::string& path);
		};
	} // namespace detail


	class Asset_manager : util::no_copy_move {
	  public:
		Asset_manager(const std::string& exe_name, const std::string& org_name, const std::string& app_name);
		~Asset_manager();

		void reload();
		void shrink_to_fit() noexcept;


		template <typename T>
		auto load(const AID& id, bool cache = true) -> Loading<T>;

		template <typename T>
		auto load_maybe(const AID& id, bool cache = true) -> util::maybe<Loading<T>>;

		template <typename T>
		void save(const AID& id, const T& asset);

		template <typename T>
		void save(const Ptr<T>& asset);


		auto exists(const AID& id) const noexcept -> bool;
		auto try_delete(const AID& id) -> bool;

		auto open(const AID& id) -> util::maybe<istream>;
		auto open_rw(const AID& id) -> ostream;

		auto list(Asset_type type) -> std::vector<AID>;
		auto last_modified(const AID& id) const noexcept -> util::maybe<std::int64_t>;

		auto resolve(const AID& id) const noexcept -> util::maybe<std::string>;
		auto resolve_reverse(std::string_view) -> util::maybe<AID>;

		template <typename T, typename... Args>
		void create_stateful_loader(Args&&... args);

		template <typename T>
		void remove_stateful_loader();

	  private:
		friend class ostream;

		template <typename T>
		friend class detail::Asset_container;

	  private:
		mutable std::mutex        _containers_mutex;
		mutable std::shared_mutex _dispatchers_mutex;

		detail::Map<util::type_uid_t, std::unique_ptr<detail::Asset_container_base>> _containers;
		detail::Map<AID, std::string>                                                _dispatchers;


		void _post_write();

		auto _base_dir(Asset_type type) const -> util::maybe<std::string>;

		void _reload_dispatchers();

		auto _last_modified(const std::string& path) const -> int64_t;
		auto _open(const asset::AID& id, const std::string& path) -> istream;
		auto _open_rw(const asset::AID& id, const std::string& path) -> ostream;

		template <typename T>
		auto _find_container() -> util::maybe<detail::Asset_container<T>&>;
	};

	template <class T>
	util::maybe<const T&> unpack(util::maybe<Ptr<T>> m) {
		return m.process(util::maybe<const T&>{}, [](Ptr<T>& p) { return util::maybe<const T&>{*p}; });
	}
} // namespace mirrage::asset

#define MIRRAGE_ASSETMANAGER_INCLUDED
#include "asset_manager.hxx"
