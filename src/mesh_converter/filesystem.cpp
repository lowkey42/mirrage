#include "filesystem.hpp"

#include <mirrage/utils/log.hpp>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
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
} // namespace mirrage
