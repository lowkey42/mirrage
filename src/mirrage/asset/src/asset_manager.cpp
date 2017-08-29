#include <mirrage/asset/asset_manager.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <physfs.h>

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

using namespace mirrage::util;

namespace {

	std::string append_file(const std::string& folder, const std::string file) {
		if(ends_with(folder, "/") || starts_with(file, "/"))
			return folder + file;
		else
			return folder + "/" + file;
	}
	void create_dir(const std::string& dir) {
#ifdef _WIN32
		CreateDirectory(dir.c_str(), NULL);
#else
		mkdir(dir.c_str(), 0777);
#endif
	}

	auto last_of(const std::string& str, char c) {
		auto idx = str.find_last_of(c);
		return idx != std::string::npos ? mirrage::util::just(idx + 1) : mirrage::util::nothing;
	}

	auto split_path(const std::string& path) {
		auto filename_delim_end = last_of(path, '/').get_or_other(last_of(path, '\\').get_or_other(0));

		return std::make_tuple(path.substr(0, filename_delim_end - 1),
		                       path.substr(filename_delim_end, std::string::npos));
	}


	std::vector<std::string> list_files(const std::string& dir,
	                                    const std::string& prefix,
	                                    const std::string& suffix) noexcept {
		std::vector<std::string> res;

		char** rc = PHYSFS_enumerateFiles(dir.c_str());

		for(char** i = rc; *i != nullptr; i++) {
			std::string str(*i);

			if((prefix.length() == 0 || str.find(prefix) == 0)
			   && (suffix.length() == 0 || str.find(suffix) == str.length() - suffix.length()))
				res.emplace_back(std::move(str));
		}

		PHYSFS_freeList(rc);

		return res;
	}
	std::vector<std::string> list_wildcard_files(const std::string& wildcard_path) {
		auto   spr  = split_path(wildcard_path);
		auto&& path = std::get<0>(spr);
		auto&& file = std::get<1>(spr);

		auto wildcard = last_of(file, '*').get_or_other(file.length());
		if(wildcard != (file.find_first_of('*') + 1)) {
			WARN("More than one wildcard ist currently not supported. Found in: " << wildcard_path);
		}

		auto prefix = file.substr(0, wildcard - 1);
		auto suffix = wildcard < file.length() ? file.substr(0, wildcard - 1) : std::string();

		auto files = list_files(path, prefix, suffix);
		for(auto& file : files) {
			file = path + "/" + file;
		}

		return files;
	}

	bool exists_file(const std::string path) {
		return PHYSFS_exists(path.c_str()) != 0 && PHYSFS_isDirectory(path.c_str()) == 0;
	}
	bool exists_dir(const std::string path) {
		return PHYSFS_exists(path.c_str()) != 0 && PHYSFS_isDirectory(path.c_str()) != 0;
	}

	template <typename Stream>
	void print_dir_recursiv(const std::string& dir, uint8_t depth, Stream& stream) {
		std::string p;
		for(uint8_t i = 0; i < depth; i++)
			p += "  ";

		stream << p << dir << "\n";
		depth++;
		for(auto&& f : list_files(dir, "", "")) {
			if(depth >= 5)
				stream << p << "  " << f << "\n";
			else
				print_dir_recursiv(f, depth, stream);
		}
	}

	constexpr auto default_source = {std::make_tuple("assets", false), std::make_tuple("assets.zip", true)};
}

namespace mirrage::asset {

	std::string pwd() {
		char cCurrentPath[FILENAME_MAX];

#ifdef WINDOWS
		_getcwd(cCurrentPath, sizeof(cCurrentPath));
#else
		getcwd(cCurrentPath, sizeof(cCurrentPath));
#endif

		return cCurrentPath;
	}


#ifdef EMSCRIPTEN
	static bool initial_sync_done = false;

	extern "C" void EMSCRIPTEN_KEEPALIVE post_sync_handler() { initial_sync_done = true; }

	void setup_storage() {
		EM_ASM(FS.mkdir('/persistent_data'); FS.mount(IDBFS, {}, '/persistent_data');

		       Module.syncdone = 0;

		       //populate persistent_data directory with existing persistent source data
		       //stored with Indexed Db
		       //first parameter = "true" mean synchronize from Indexed Db to
		       //Emscripten file system,
		       // "false" mean synchronize from Emscripten file system to Indexed Db
		       //second parameter = function called when data are synchronized
		       FS.syncfs(true, function(err) {
			       //assert(!err);
			       Module.print("end file sync..");
			       Module.syncdone = 1;
			       ccall('post_sync_handler', 'v');
			   }););
	}

	bool storage_ready() { return initial_sync_done; }
#else
	void setup_storage() {}
	bool storage_ready() { return true; }

#endif

	static Asset_manager* current_instance = nullptr;
	auto                  get_asset_manager() -> Asset_manager& {
		INVARIANT(current_instance != nullptr, "Asset_manager has not been initialized!");
		return *current_instance;
	}

	Asset_manager::Asset_manager(const std::string& exe_name, const std::string& app_name) {
		if(!PHYSFS_init(exe_name.empty() ? nullptr : exe_name.c_str()))
			FAIL("PhysFS-Init failed for \"" << exe_name << "\": " << PHYSFS_getLastError());

		// TODO: Windows savegames should be stored in FOLDERID_SavedGames, but the API and conventions are a pain in the ass
		std::string write_dir_parent = append_file(PHYSFS_getUserDir(),
#ifdef _WIN32
		                                           "Documents/My Games"
#else
		                                           ".config"
#endif
		                                           );

#ifdef EMSCRIPTEN
		INVARIANT(storage_ready(), "Storage is not ready");
		write_dir_parent = "/persistent_data";
#endif

		if(!PHYSFS_addToSearchPath(PHYSFS_getBaseDir(), 1)
		   || !PHYSFS_addToSearchPath(append_file(PHYSFS_getBaseDir(), "..").c_str(), 1)
		   || !PHYSFS_addToSearchPath(pwd().c_str(), 1))
			FAIL("Unable to construct search path: " << PHYSFS_getLastError());

		// add optional search path
		PHYSFS_addToSearchPath(
		        append_file(append_file(append_file(PHYSFS_getBaseDir(), ".."), app_name), "assets").c_str(),
		        1);


		if(exists_dir("write_dir")) {
			write_dir_parent = "write_dir";
		}

		create_dir(write_dir_parent);

		std::string write_dir = append_file(write_dir_parent, app_name);
		create_dir(write_dir);

		INFO("Write dir: " << write_dir);

		if(!PHYSFS_addToSearchPath(write_dir.c_str(), 0))
			FAIL("Unable to construct search path: " << PHYSFS_getLastError());

		if(!PHYSFS_setWriteDir(write_dir.c_str()))
			FAIL("Unable to set write-dir to \"" << write_dir << "\": " << PHYSFS_getLastError());


		auto add_source = [](const char* path) {
			if(!PHYSFS_addToSearchPath(path, 1))
				WARN("Error adding custom archive \"" << path << "\": " << PHYSFS_getLastError());
		};

		auto archive_file = _open("archives.lst");
		if(!archive_file) {
			bool lost = true;
			for(auto& s : default_source) {
				const char* path;
				bool        file;

				std::tie(path, file) = s;

				if(file ? exists_file(path) : exists_dir(path)) {
					add_source(path);
					lost = false;
				}
			}

			if(lost) {
				auto& log = util::fail(__func__, __FILE__, __LINE__);
				log << "No archives.lst found. printing search-path...\n";
				print_dir_recursiv("/", 0, log);

				log << std::endl; // crash with error

			} else {
				INFO("No archives.lst found. Using defaults.");
			}

		} else {
			// load other archives
			archive_file.process([&](istream& in) {
				for(auto&& l : in.lines()) {
					if(l.find_last_of('*') != std::string::npos) {
						for(auto& file : list_wildcard_files(l)) {
							add_source(file.c_str());
						}
						continue;
					}
					add_source(l.c_str());
				}
			});
		}

		_reload_dispatchers();

		current_instance = this;
	}

	Asset_manager::~Asset_manager() {
		current_instance = nullptr;
		_assets.clear();
		PHYSFS_deinit();
	}

	void Asset_manager::_reload_dispatchers() {
		_dispatcher.clear();

		for(auto&& df : list_files("", "assets", ".map")) {
			_open(df).process([this](istream& in) {
				for(auto&& l : in.lines()) {
					auto        kvp  = util::split(l, "=");
					std::string path = util::trim_copy(kvp.second);
					if(!path.empty()) {
						_dispatcher.emplace(AID{kvp.first}, std::move(path));
					}
				}
			});
		}
	}

	void Asset_manager::_post_write() {
#ifdef EMSCRIPTEN
		//persist Emscripten current data to Indexed Db
		EM_ASM(FS.syncfs(false,
		                 function(err){
		                         //assert(!err);
		                 }););
#endif
		reload();
	}

	util::maybe<std::string> Asset_manager::_base_dir(Asset_type type) const {
		auto dir = _dispatcher.find(AID{type, ""}); // search for prefix-entry

		if(dir == _dispatcher.end())
			return util::nothing;

		std::string bdir = dir->second;
		return bdir;
	}

	std::vector<AID> Asset_manager::list(Asset_type type) {
		std::vector<AID> res;

		for(auto& d : _dispatcher) {
			if(d.first.type() == type && d.first.name().size() > 0)
				res.emplace_back(d.first);
		}

		_base_dir(type).process([&](const std::string& dir) {
			for(auto&& f : list_files(dir, "", ""))
				res.emplace_back(type, f);
		});

		sort(res.begin(), res.end());
		res.erase(std::unique(res.begin(), res.end()), res.end());

		return res;
	}

	util::maybe<istream> Asset_manager::_open(const std::string& path) {
		return _open(path, AID{"gen"_strid, path});
	}
	util::maybe<istream> Asset_manager::_open(const std::string& path, const AID& aid) {
		return exists_file(path) ? util::just(istream{aid, *this, path}) : util::nothing;
	}

	Asset_manager::Asset::Asset(std::shared_ptr<void> data, Reloader reloader, int64_t last_modified)
	  : data(data), reloader(reloader), last_modified(last_modified) {}

	void Asset_manager::_add_asset(const AID&            id,
	                               const std::string&    path,
	                               Reloader              reloader,
	                               std::shared_ptr<void> asset) {
		_assets.emplace(id, Asset{asset, reloader, PHYSFS_getLastModTime(path.c_str())});
	}

	auto Asset_manager::_locate(const AID& id, bool warn) const -> std::tuple<Location_type, std::string> {
		auto res = _dispatcher.find(id);

		if(res != _dispatcher.end()) {
			if(exists_file(res->second))
				return std::make_tuple(Location_type::file, res->second);
			else if(util::contains(res->second, ":"))
				return std::make_tuple(Location_type::indirection, res->second);
			else if(warn)
				INFO("Asset not found in configured place: " << res->second);
		}

		if(exists_file(id.name()))
			return std::make_tuple(Location_type::file, id.name());

		auto baseDir = _base_dir(id.type());

		if(baseDir.is_some()) {
			auto path = append_file(baseDir.get_or_throw(), id.name());
			if(exists_file(path))
				return std::make_tuple(Location_type::file, std::move(path));
			else if(warn)
				DEBUG("asset " << id.str() << " not found in " << path);
		}

		return std::make_tuple(Location_type::none, std::string());
	}

	ostream Asset_manager::_create(const AID& id) {
		std::string path;

		auto path_res = _dispatcher.find(id);
		if(path_res != _dispatcher.end())
			path = path_res->second;

		else {
			auto res = _dispatcher.find(AID{id.type(), ""}); // search for prefix-entry

			if(res != _dispatcher.end()) {
				PHYSFS_mkdir(res->second.c_str());
				path = append_file(res->second, id.name());
			} else {
				path = id.name();
			}
		}

		if(exists_file(path))
			PHYSFS_delete(path.c_str());

		return {id, *this, path};
	}

	auto Asset_manager::physical_location(const AID& id, bool warn) const noexcept
	        -> util::maybe<std::string> {
		using namespace std::literals;

		Location_type type;
		std::string   location;
		std::tie(type, location) = _locate(id, warn);

		if(type != Location_type::file)
			return util::nothing;

		auto dir = PHYSFS_getRealDir(location.c_str());
		if(!dir)
			return util::nothing;

		auto file = dir + "/"s + location;
		return exists_file(file) ? util::just(std::move(file)) : util::nothing;
	}

	auto Asset_manager::last_modified(const AID& id) const noexcept -> util::maybe<std::int64_t> {
		using namespace std::literals;

		Location_type type;
		std::string   location;
		std::tie(type, location) = _locate(id, false);

		if(type != Location_type::file)
			return util::nothing;

		return PHYSFS_getLastModTime(location.c_str());
	}

	auto Asset_manager::try_delete(const AID& id) -> bool {
		using namespace std::literals;

		Location_type type;
		std::string   location;
		std::tie(type, location) = _locate(id, false);

		if(type != Location_type::file)
			return false;

		return PHYSFS_delete(location.c_str());
	}

	void Asset_manager::reload() {
		_reload_dispatchers();
		for(auto& a : _assets) {
			Location_type type;
			std::string   location;
			std::tie(type, location) = _locate(a.first);

			if(type == Location_type::file) {
				auto last_mod = PHYSFS_getLastModTime(location.c_str());
				if(last_mod != -1 && last_mod > a.second.last_modified) {
					_open(location, a.first).process([&](istream& in) {
						DEBUG("Reload: " << a.first.str());
						try {
							a.second.reloader(a.second.data.get(), std::move(in));

						} catch(Loading_failed&) {}

						a.second.last_modified = last_mod;
					});
				}
			}
		}

		for(auto& w : _watchlist) {
			_check_watch_entry(w);
		}
	}
	void Asset_manager::_force_reload(const AID& aid) {
		auto iter = _assets.find(aid);
		if(iter == _assets.end())
			return;

		Location_type type;
		std::string   location;
		std::tie(type, location) = _locate(aid);

		if(type == Location_type::file) {
			_open(location, aid).process([&](istream& in) {
				try {
					iter->second.reloader(iter->second.data.get(), std::move(in));
					for(auto& w : _watchlist) {
						if(w.aid == aid) {
							w.on_mod(aid);
							w.last_modified = PHYSFS_getLastModTime(location.c_str());
						}
					}

				} catch(Loading_failed&) {}
			});
		}
	}

	void Asset_manager::shrink_to_fit() noexcept {
		util::erase_if(_assets, [](const auto& v) { return v.second.data.use_count() <= 1; });
	}

	bool Asset_manager::exists(const AID& id) const noexcept {
		Location_type type;
		std::string   location;
		std::tie(type, location) = _locate(id);

		switch(type) {
			case Location_type::none: return false;

			case Location_type::file: return exists_file(location);

			case Location_type::indirection: return true;
		}

		FAIL("Unexpected Location_type: " << static_cast<int>(type));
	}

	auto Asset_manager::load_raw(const AID& id) -> util::maybe<istream> {
		Location_type type;
		std::string   path;
		std::tie(type, path) = _locate(id);

		if(type != Location_type::file)
			return util::nothing;

		return _open(path, id);
	}
	auto Asset_manager::save_raw(const AID& id) -> ostream {
		_assets.erase(id);
		return _create(id);
	}

	auto Asset_manager::find_by_path(const std::string& path) -> util::maybe<AID> {
		static const auto working_dir  = util::replace(pwd(), "\\", "/");
		auto              path_cleared = util::replace(util::replace(path, "\\", "/"), working_dir + "/", "");

		for(auto& aid_path : _dispatcher) {
			auto loc = physical_location(aid_path.first, false);
			if(loc.is_some()) {
				if(loc.get_or_throw() == path_cleared) {
					return util::justCopy(aid_path.first);
				}
			}
		}

		DEBUG("Couldn't finde asset for '" << path_cleared << "'");

		return util::nothing;
	}

	auto Asset_manager::watch(AID aid, std::function<void(const AID&)> on_mod) -> uint32_t {
		auto id = _next_watch_id++;
		_watchlist.emplace_back(id, aid, std::move(on_mod));
		return id;
	}

	void Asset_manager::unwatch(uint32_t id) {
		auto iter = std::find_if(_watchlist.begin(), _watchlist.end(), [id](auto& w) { return w.id == id; });

		if(iter != _watchlist.end()) {
			if(&_watchlist.back() != &*iter) {
				_watchlist.back() = std::move(*iter);
			}

			_watchlist.pop_back();
		}
	}

	void Asset_manager::_check_watch_entry(Watch_entry& w) {
		Location_type type;
		std::string   location;
		std::tie(type, location) = _locate(w.aid);

		if(type == Location_type::file) {
			auto last_mod = PHYSFS_getLastModTime(location.c_str());
			if(last_mod != -1 && last_mod > w.last_modified) {
				w.last_modified = last_mod;
				w.on_mod(w.aid);
			}
		}
	}
}
