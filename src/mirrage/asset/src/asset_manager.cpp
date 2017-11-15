#include <mirrage/asset/asset_manager.hpp>

#include <mirrage/asset/error.hpp>

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
using namespace std::string_literals;

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
		auto filename_delim_end = last_of(path, '/').get_or(last_of(path, '\\').get_or(0));

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

		auto wildcard = last_of(file, '*').get_or(file.length());
		if(wildcard != (file.find_first_of('*') + 1)) {
			MIRRAGE_WARN("More than one wildcard ist currently not supported. Found in: "
			             << wildcard_path);
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
		if(PHYSFS_exists(path.c_str()) == 0)
			return false;

		auto stat = PHYSFS_Stat{};
		if(PHYSFS_stat(path.c_str(), &stat) == 0)
			return false;

		return stat.filetype == PHYSFS_FILETYPE_REGULAR;
	}
	bool exists_dir(const std::string path) {
		if(PHYSFS_exists(path.c_str()) == 0)
			return false;

		auto stat = PHYSFS_Stat{};
		if(PHYSFS_stat(path.c_str(), &stat) == 0)
			return false;

		return stat.filetype == PHYSFS_FILETYPE_DIRECTORY;
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

	constexpr auto default_source = {std::make_tuple("assets", false),
	                                 std::make_tuple("assets.zip", true)};
} // namespace

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


	Asset_manager::Asset_manager(const std::string& exe_name,
	                             const std::string& org_name,
	                             const std::string& app_name) {
		if(!PHYSFS_init(exe_name.empty() ? nullptr : exe_name.c_str()))
			throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to initalize PhysicsFS.");

		auto write_dir = PHYSFS_getPrefDir(org_name.c_str(), app_name.c_str());

#ifdef EMSCRIPTEN
		MIRRAGE_INVARIANT(storage_ready(), "Storage is not ready");
		write_dir = "/persistent_data";
#endif

		if(!PHYSFS_mount(PHYSFS_getBaseDir(), nullptr, 1)
		   || !PHYSFS_mount(append_file(PHYSFS_getBaseDir(), "..").c_str(), nullptr, 1)
		   || !PHYSFS_mount(pwd().c_str(), nullptr, 1))
			throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to setup default search path.");


		if(exists_dir("write_dir")) {
			write_dir = "write_dir";
		}

		create_dir(write_dir);

		MIRRAGE_INFO("Write dir: " << write_dir);

		if(!PHYSFS_mount(write_dir, nullptr, 0))
			throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to construct search path.");

		if(!PHYSFS_setWriteDir(write_dir))
			throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to set write-dir: "s + write_dir);


		auto add_source = [](const char* path) {
			if(!PHYSFS_mount(path, nullptr, 1))
				throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
				                        "Error adding custom archive: "s + path);
		};

		if(exists_file("archives.lst")) {
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
				auto& log = util::error(__func__, __FILE__, __LINE__);
				log << "No archives.lst found. printing search-path...\n";
				print_dir_recursiv("/", 0, log);
				log << std::endl;

				throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
				                        "No archives.lst found.");

			} else {
				MIRRAGE_INFO("No archives.lst found. Using defaults.");
			}

		} else {
			auto in = _open("cfg:archives.lst"_aid, "archives.lst");

			// load other archives
			for(auto&& l : in.lines()) {
				if(l.find_last_of('*') != std::string::npos) {
					for(auto& file : list_wildcard_files(l)) {
						add_source(file.c_str());
					}
					continue;
				}
				add_source(l.c_str());
			}
		}

		_reload_dispatchers();
	}

	Asset_manager::~Asset_manager() {
		_containers.clear();
		if(!PHYSFS_deinit()) {
			MIRRAGE_FAIL("Unable to shutdown PhysicsFS: "
			             << PHYSFS_getErrorByCode((PHYSFS_getLastErrorCode())));
		}
	}

	void Asset_manager::reload() {
		_reload_dispatchers();

		// The container lock must not be held during reload, because the reload of an asset calls
		//   third-party code that might call into the asset_manager.
		// So we first collect all relevant containers and then iterate over that list.
		auto containers = std::vector<detail::Asset_container_base*>();
		{
			auto lock = std::scoped_lock{_containers_mutex};
			containers.reserve(_containers.size());

			for(auto& container : _containers) {
				containers.emplace_back(container.second.get());
			}
		}

		for(auto& c : containers) {
			c->reload();
		}
	}
	void Asset_manager::shrink_to_fit() noexcept {
		auto lock = std::scoped_lock{_containers_mutex};

		for(auto& container : _containers) {
			container.second->shrink_to_fit();
		}
	}

	auto Asset_manager::exists(const AID& id) const noexcept -> bool {
		return resolve(id).process(false, [](auto&& path) { return exists_file(path); });
	}
	auto Asset_manager::try_delete(const AID& id) -> bool {
		return resolve(id).process(true,
		                           [](auto&& path) { return PHYSFS_delete(path.c_str()) == 0; });
	}

	auto Asset_manager::open(const AID& id) -> util::maybe<istream> {
		auto path = resolve(id);

		if(path.is_some() && exists_file(path.get_or_throw()))
			return _open(id, path.get_or_throw());
		else
			return util::nothing;
	}
	auto Asset_manager::open_rw(const AID& id) -> ostream {
		auto path = resolve(id);
		if(path.is_nothing()) {
			path = resolve(AID{id.type(), ""}).process(id.str(), [&](auto&& prefix) {
				return append_file(prefix, id.str());
			});
		};

		if(exists_file(path.get_or_throw()))
			PHYSFS_delete(path.get_or_throw().c_str());

		return _open_rw(id, path.get_or_throw());
	}

	auto Asset_manager::list(Asset_type type) -> std::vector<AID> {
		auto lock = std::shared_lock{_dispatchers_mutex};

		std::vector<AID> res;

		for(auto& d : _dispatchers) {
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
	auto Asset_manager::last_modified(const AID& id) const noexcept -> util::maybe<std::int64_t> {
		using namespace std::literals;

		return resolve(id).process([&](auto& path) { return _last_modified(path); });
	}

	auto Asset_manager::resolve(const AID& id) const noexcept -> util::maybe<std::string> {
		auto lock = std::shared_lock{_dispatchers_mutex};

		auto res = _dispatchers.find(id);

		if(res != _dispatchers.end() && exists_file(res->second))
			return res->second;

		else if(exists_file(id.name()))
			return id.name();


		auto baseDir = _base_dir(id.type());

		if(baseDir.is_some()) {
			auto path = append_file(baseDir.get_or_throw(), id.name());
			if(exists_file(path))
				return std::move(path);
		}

		return util::nothing;
	}
	auto Asset_manager::resolve_reverse(std::string_view path) -> util::maybe<AID> {
		auto lock = std::shared_lock{_dispatchers_mutex};

		for(auto& e : _dispatchers) {
			if(e.second == path)
				return e.first;
		}

		return util::nothing;
	}

	void Asset_manager::_post_write() {
#ifdef EMSCRIPTEN
		//persist Emscripten current data to Indexed Db
		EM_ASM(FS.syncfs(false,
		                 function(err){
		                         //assert(!err);
		                 }););
#endif
	}

	auto Asset_manager::_base_dir(Asset_type type) const -> util::maybe<std::string> {
		auto lock = std::shared_lock{_dispatchers_mutex};

		auto dir = _dispatchers.find(AID{type, ""}); // search for prefix-entry

		if(dir == _dispatchers.end())
			return util::nothing;

		std::string bdir = dir->second;
		return bdir;
	}

	void Asset_manager::_reload_dispatchers() {
		auto lock = std::unique_lock{_dispatchers_mutex};

		_dispatchers.clear();

		for(auto&& df : list_files("", "assets", ".map")) {
			auto in = _open({}, df);
			for(auto&& l : in.lines()) {
				auto        kvp  = util::split(l, "=");
				std::string path = util::trim_copy(kvp.second);
				if(!path.empty()) {
					_dispatchers.emplace(AID{kvp.first}, std::move(path));
				}
			}
		}
	}

	auto Asset_manager::_last_modified(const std::string& path) const -> int64_t {
		auto stat = PHYSFS_Stat{};
		if(auto errc = PHYSFS_stat(path.c_str(), &stat); errc != 0)
			throw std::system_error(static_cast<Asset_error>(errc));

		return stat.modtime;
	}
	auto Asset_manager::_open(const asset::AID& id, const std::string& path) -> istream {
		return {id, *this, path};
	}
	auto Asset_manager::_open_rw(const asset::AID& id, const std::string& path) -> ostream {
		return {id, *this, path};
	}

} // namespace mirrage::asset
