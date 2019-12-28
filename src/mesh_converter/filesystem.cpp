#include "filesystem.hpp"

#include <mirrage/utils/log.hpp>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define access _access_s
#else
#include <sys/stat.h>
#include <unistd.h>
#endif


namespace mirrage {

	void create_directory(const std::string& dir)
	{
#ifdef _WIN32
		CreateDirectory(dir.c_str(), NULL);
#else
		auto status = mkdir(dir.c_str(), 0755);
		MIRRAGE_INVARIANT(status == 0 || errno == EEXIST,
		                  "Couldn't create directory \"" << dir << "\": " << int(errno));
#endif
	}

	auto exists(const std::string& name) -> bool { return access(name.c_str(), 0) == 0; }

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

} // namespace mirrage
