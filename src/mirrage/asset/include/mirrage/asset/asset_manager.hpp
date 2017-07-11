/** interface for loading & writing asset & files ****************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/aid.hpp>
#include <mirrage/asset/stream.hpp>

#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/template_utils.hpp>
#include <mirrage/utils/stacktrace.hpp>
#include <mirrage/utils/string_utils.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <iostream>

/**
 * void example(asset::Manager& assetMgr) {
 *		asset::Ptr<Texture> itemTex = assetMgr.load<Texture>("tex:items/health/small"_aid);
 * }
 */
namespace mirrage {
namespace asset {
	class Asset_manager;

	extern std::string pwd();

	extern void setup_storage();
	extern bool storage_ready();

	template<class R>
	class Ptr {
		public:
			Ptr();
			Ptr(Asset_manager& mgr, const AID& id, std::shared_ptr<const R> res=std::shared_ptr<const R>());

			bool operator==(const Ptr& o)const noexcept;
			bool operator<(const Ptr& o)const noexcept;

			auto operator*() -> const R&;
			auto operator*()const -> const R&;

			auto operator->() -> const R*;
			auto operator->()const -> const R*;

			operator bool()const noexcept {return !!_ptr;}
			operator std::shared_ptr<const R>();

			auto aid()const noexcept -> const AID& {return _aid;}
			auto mgr()const noexcept -> Asset_manager& {return *_mgr;}
			void load();
			auto try_load(bool cache=true, bool warn=true) -> bool;
			void unload();

			void reset(){_ptr.reset();}

		private:
			Asset_manager* _mgr;
			std::shared_ptr<const R> _ptr;
			AID _aid;
	};

	extern auto get_asset_manager() -> Asset_manager&;

	class Asset_manager : util::no_copy_move {
		public:
			Asset_manager(const std::string& exe_name, const std::string& app_name);
			~Asset_manager();

			void shrink_to_fit()noexcept;

			template<typename T>
			auto load(const AID& id, bool cache=true) -> Ptr<T>;

			template<typename T>
			auto load_maybe(const AID& id, bool cache=true, bool warn=true) -> util::maybe<Ptr<T>>;

			auto load_raw(const AID& id) -> util::maybe<istream>;

			auto list(Asset_type type) -> std::vector<AID>;

			auto find_by_path(const std::string&) -> util::maybe<AID>;

			template<typename T>
			void save(const AID& id, const T& asset);

			auto save_raw(const AID& id) -> ostream;

			bool exists(const AID& id)const noexcept;

			auto try_delete(const AID& id) -> bool;

			auto physical_location(const AID& id, bool warn=true)const noexcept -> util::maybe<std::string>;

			auto last_modified(const AID& id)const noexcept -> util::maybe<std::int64_t>;

			void reload();

			auto watch(AID aid, std::function<void(const AID&)> on_mod) -> uint32_t;
			void unwatch(uint32_t id);


		private:
			friend class ostream;

			using Reloader = void (*)(void*, istream);

			template<class T>
			static void _asset_reloader_impl(void* asset, istream in);

			struct Asset {
				std::shared_ptr<void> data;
				Reloader reloader;
				int64_t last_modified;

				Asset(std::shared_ptr<void> data, Reloader reloader, int64_t last_modified);
			};
			enum class Location_type {
				none, file, indirection
			};
			struct Watch_entry {
				uint32_t id;
				AID aid;
				std::function<void(const AID&)> on_mod;
				int64_t last_modified = 0;

				Watch_entry(uint32_t id, AID aid, std::function<void(const AID&)> l)
				    : id(id), aid(aid), on_mod(l) {}
			};

			std::unordered_map<AID, Asset> _assets;
			std::unordered_map<AID, std::string> _dispatcher;
			std::vector<Watch_entry> _watchlist;
			uint32_t _next_watch_id = 0;

			void _add_asset(const AID& id, const std::string& path, Reloader reloader, std::shared_ptr<void> asset);

			auto _base_dir(Asset_type type)const -> util::maybe<std::string>;
			auto _open(const std::string& path) -> util::maybe<istream>;
			auto _open(const std::string& path, const AID& aid) -> util::maybe<istream>;
			auto _locate(const AID& id, bool warn=true)const -> std::tuple<Location_type, std::string>;

			auto _create(const AID& id) -> ostream;
			void _post_write();
			void _reload_dispatchers();
			void _force_reload(const AID& aid);
			void _check_watch_entry(Watch_entry&);
	};

	template<class T>
	util::maybe<const T&> unpack(util::maybe<Ptr<T>> m) {
		return m.process(util::maybe<const T&>{}, [](Ptr<T>& p){return util::maybe<const T&>{*p};});
	}

} /* namespace asset */
}

#define ASSETMANAGER_INCLUDED
#include "asset_manager.hxx"
