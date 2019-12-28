#pragma once

#include <string>


namespace mirrage {

	extern void create_directory(const std::string& name);

	extern auto exists(const std::string& name) -> bool;

	extern std::string pwd();

} // namespace mirrage
