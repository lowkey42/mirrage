#include <mirrage/asset/asset_manager.hpp>

#include <mirrage/asset/embedded_asset.hpp>
#include <mirrage/asset/error.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/md5.hpp>
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


using namespace mirrage::util;
using namespace std::string_literals;

namespace {

	std::string append_file(const std::string& folder, const std::string file)
	{
		if(ends_with(folder, "/") || starts_with(file, "/"))
			return folder + file;
		else
			return folder + "/" + file;
	}
	void create_dir(const std::string& dir)
	{
#ifdef _WIN32
		CreateDirectory(dir.c_str(), NULL);
#else
		mkdir(dir.c_str(), 0777);
#endif
	}

	auto last_of(const std::string& str, char c)
	{
		auto idx = str.find_last_of(c);
		return idx != std::string::npos ? mirrage::util::just(idx + 1) : mirrage::util::nothing;
	}

	auto split_path(const std::string& path)
	{
		auto filename_delim_end = last_of(path, '/').get_or(last_of(path, '\\').get_or(0));

		return std::make_tuple(path.substr(0, filename_delim_end - 1),
		                       path.substr(filename_delim_end, std::string::npos));
	}


	std::vector<std::string> list_files(const std::string& dir,
	                                    const std::string& prefix,
	                                    const std::string& suffix) noexcept
	{
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
	std::vector<std::string> list_wildcard_files(const std::string& wildcard_path)
	{
		auto   spr  = split_path(wildcard_path);
		auto&& path = std::get<0>(spr);
		auto&& file = std::get<1>(spr);

		auto wildcard = last_of(file, '*').get_or(file.length());
		LOG_IF(plog::warning, wildcard != file.find_first_of('*') + 1)
		        << "More than one wildcard ist currently not supported. Found in: " << wildcard_path;

		auto prefix = file.substr(0, wildcard - 1);
		auto suffix = wildcard < file.length() ? file.substr(0, wildcard - 1) : std::string();

		auto files = list_files(path, prefix, suffix);
		for(auto& file : files) {
			file = path + "/" + file;
		}

		return files;
	}

	bool exists_file(const std::string path)
	{
		if(!PHYSFS_exists(path.c_str()))
			return false;

		auto stat = PHYSFS_Stat{};
		if(!PHYSFS_stat(path.c_str(), &stat))
			return false;

		return stat.filetype == PHYSFS_FILETYPE_REGULAR;
	}
	bool exists_dir(const std::string path)
	{
		if(!PHYSFS_exists(path.c_str()))
			return false;

		auto stat = PHYSFS_Stat{};
		if(!PHYSFS_stat(path.c_str(), &stat))
			return false;

		return stat.filetype == PHYSFS_FILETYPE_DIRECTORY;
	}

	template <typename Callback>
	void print_dir_recursiv(const std::string& dir, uint8_t depth, Callback&& callback)
	{
		std::string p;
		for(uint8_t i = 0; i < depth; i++)
			p += "  ";

		callback(p + dir);
		depth++;
		for(auto&& f : list_files(dir, "", "")) {
			if(depth >= 5)
				callback(p + "  " + f);
			else
				print_dir_recursiv(f, depth, callback);
		}
	}

	void init_physicsfs(const std::string& exe_name, mirrage::util::maybe<std::string> additional_search_path)
	{
		if(PHYSFS_isInit())
			return;

		if(!PHYSFS_init(exe_name.empty() ? nullptr : exe_name.c_str())) {
			throw std::system_error(static_cast<mirrage::asset::Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to initalize PhysicsFS.");
		}

		if(!PHYSFS_mount(PHYSFS_getBaseDir(), nullptr, 1)
		   || !PHYSFS_mount(append_file(PHYSFS_getBaseDir(), "..").c_str(), nullptr, 1)
		   || !PHYSFS_mount(mirrage::asset::pwd().c_str(), nullptr, 1))
			throw std::system_error(static_cast<mirrage::asset::Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to setup default search path.");

		additional_search_path.process([&](auto& dir) { PHYSFS_mount(dir.c_str(), nullptr, 1); });
	}

	constexpr auto default_source = {std::make_tuple("assets", false), std::make_tuple("assets.zip", true)};
} // namespace

namespace mirrage::asset {

	std::string pwd()
	{
		char cCurrentPath[FILENAME_MAX];

#ifdef _WIN32
		_getcwd(cCurrentPath, sizeof(cCurrentPath));
#else
		if(getcwd(cCurrentPath, sizeof(cCurrentPath)) == nullptr) {
			MIRRAGE_FAIL("getcwd with max length " << FILENAME_MAX << " failed with error code " << errno);
		}
#endif

		return cCurrentPath;
	}
	std::string write_dir(const std::string&       exe_name,
	                      const std::string&       org_name,
	                      const std::string&       app_name,
	                      util::maybe<std::string> additional_search_path)
	{
		init_physicsfs(exe_name, additional_search_path);

		if(exists_dir("write_dir")) {
			return std::string(PHYSFS_getRealDir("write_dir")) + "/write_dir";
		}

		return PHYSFS_getPrefDir(org_name.c_str(), app_name.c_str());
	}

	Asset_manager::Asset_manager(const std::string&       exe_name,
	                             const std::string&       org_name,
	                             const std::string&       app_name,
	                             util::maybe<std::string> additional_search_path)
	{
		init_physicsfs(exe_name, additional_search_path);

		auto write_dir = ::mirrage::asset::write_dir(exe_name, org_name, app_name, additional_search_path);
		create_dir(write_dir);
		LOG(plog::debug) << "Write dir: " << write_dir;

		if(!PHYSFS_mount(write_dir.c_str(), nullptr, 0))
			throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to construct search path.");

		if(!PHYSFS_setWriteDir(write_dir.c_str()))
			throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
			                        "Unable to set write-dir: "s + write_dir);

		for(auto ea : Embedded_asset::instances()) {
			LOG(plog::info) << "Include embedded asset \"" << ea->name() << "\": " << ea->data().size()
			                << " bytes MD5: "
			                << util::md5(std::string(reinterpret_cast<const char*>(ea->data().data()),
			                                         std::size_t(ea->data().size_bytes())));
			auto name = "embedded_" + ea->name() + ".zip";
			if(!PHYSFS_mountMemory(ea->data().data(),
			                       static_cast<PHYSFS_uint64>(ea->data().size_bytes()),
			                       nullptr,
			                       name.c_str(),
			                       nullptr,
			                       1)) {
				throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
				                        "Unable to add embedded archive: "s + ea->name());
			}
		}

		auto add_source = [](std::string path) {
			auto apath = PHYSFS_getRealDir(path.c_str());
			if(apath) {
				path = std::string(apath) + "/" + path;
			}

			LOG(plog::info) << "Added FS directory: " << path;
			if(!PHYSFS_mount(path.c_str(), nullptr, 1))
				throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
				                        "Error adding custom archive: "s + path);
		};

		if(!exists_file("archives.lst")) {
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
				LOG(plog::fatal) << "No archives.lst found. printing search-path...\n";
				print_dir_recursiv("/", 0, [](auto&& path) { LOG(plog::fatal) << path; });

				throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()),
				                        "No archives.lst found.");
			} else {
				LOG(plog::info) << "No archives.lst found. Using defaults.";
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

		// unmount default search-path
		PHYSFS_unmount(PHYSFS_getBaseDir());
		PHYSFS_unmount(append_file(PHYSFS_getBaseDir(), "..").c_str());
		PHYSFS_unmount(mirrage::asset::pwd().c_str());
		additional_search_path.process([&](auto& dir) { PHYSFS_unmount(dir.c_str()); });

		_reload_dispatchers();
	}

	Asset_manager::~Asset_manager()
	{
		_containers.clear();
		if(!PHYSFS_deinit()) {
			MIRRAGE_FAIL(
			        "Unable to shutdown PhysicsFS: " << PHYSFS_getErrorByCode((PHYSFS_getLastErrorCode())));
		}
	}

	void Asset_manager::reload()
	{
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
	void Asset_manager::shrink_to_fit() noexcept
	{
		auto lock = std::scoped_lock{_containers_mutex};

		for(auto& container : _containers) {
			container.second->shrink_to_fit();
		}
	}

	auto Asset_manager::exists(const AID& id) const noexcept -> bool
	{
		return resolve(id).process(false, [](auto&& path) { return exists_file(path); });
	}
	auto Asset_manager::try_delete(const AID& id) -> bool
	{
		return resolve(id).process(true, [](auto&& path) { return PHYSFS_delete(path.c_str()) == 0; });
	}

	auto Asset_manager::open(const AID& id) -> util::maybe<istream>
	{
		auto path = resolve(id);

		if(path.is_some() && exists_file(path.get_or_throw()))
			return _open(id, path.get_or_throw());
		else
			return util::nothing;
	}
	auto Asset_manager::open_rw(const AID& id) -> ostream
	{
		auto path = resolve(id, false);
		if(path.is_nothing()) {
			path = resolve(AID{id.type(), ""}).process(id.str(), [&](auto&& prefix) {
				return append_file(prefix, id.str());
			});
		};

		if(exists_file(path.get_or_throw()))
			PHYSFS_delete(path.get_or_throw().c_str());

		return _open_rw(id, path.get_or_throw());
	}

	auto Asset_manager::list(Asset_type type) -> std::vector<AID>
	{
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
	auto Asset_manager::last_modified(const AID& id) const noexcept -> util::maybe<std::int64_t>
	{
		using namespace std::literals;

		return resolve(id).process([&](auto& path) { return _last_modified(path); });
	}

	auto Asset_manager::resolve(const AID& id, bool only_preexisting) const noexcept
	        -> util::maybe<std::string>
	{
		auto lock = std::shared_lock{_dispatchers_mutex};

		auto res = _dispatchers.find(id);

		if(res != _dispatchers.end() && (exists_file(res->second) || !only_preexisting))
			return res->second;

		else if(exists_file(id.name()))
			return id.name();


		auto baseDir = _base_dir(id.type());

		if(baseDir.is_some()) {
			auto path = append_file(baseDir.get_or_throw(), id.name());
			if(exists_file(path))
				return std::move(path);

			else if(!only_preexisting) {
				PHYSFS_mkdir(baseDir.get_or_throw().c_str());
				return std::move(path);
			}
		}

		if(!only_preexisting) {
			return id.name();
		}

		return util::nothing;
	}
	auto Asset_manager::resolve_reverse(std::string_view path) -> util::maybe<AID>
	{
		auto lock = std::shared_lock{_dispatchers_mutex};

		for(auto& e : _dispatchers) {
			if(e.second == path)
				return e.first;
		}

		return util::nothing;
	}

	void Asset_manager::_post_write() {}

	auto Asset_manager::_base_dir(Asset_type type) const -> util::maybe<std::string>
	{
		auto lock = std::shared_lock{_dispatchers_mutex};

		auto dir = _dispatchers.find(AID{type, ""}); // search for prefix-entry

		if(dir == _dispatchers.end())
			return util::nothing;

		std::string bdir = dir->second;
		return bdir;
	}

	void Asset_manager::_reload_dispatchers()
	{
		auto lock = std::unique_lock{_dispatchers_mutex};

		_dispatchers.clear();

		for(auto&& df : list_files("", "assets", ".map")) {
			LOG(plog::info) << "Added asset mapping: " << df;
			auto in = _open({}, df);
			for(auto&& l : in.lines()) {
				auto        kvp  = util::split(l, "=");
				std::string path = util::trim_copy(kvp.second);
				if(!path.empty()) {
					LOG(plog::debug) << "    " << AID{kvp.first}.str() << " = " << path;
					_dispatchers.emplace(AID{kvp.first}, std::move(path));
				}
			}
		}
	}

	auto Asset_manager::_last_modified(const std::string& path) const -> int64_t
	{
		auto stat = PHYSFS_Stat{};
		if(!PHYSFS_stat(path.c_str(), &stat))
			throw std::system_error(static_cast<Asset_error>(PHYSFS_getLastErrorCode()));

		return stat.modtime;
	}
	auto Asset_manager::_open(const asset::AID& id, const std::string& path) -> istream
	{
		return {id, *this, path};
	}
	auto Asset_manager::_open_rw(const asset::AID& id, const std::string& path) -> ostream
	{
		return {id, *this, path};
	}

} // namespace mirrage::asset
