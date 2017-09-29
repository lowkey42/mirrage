/**  Access to informations from the build environment ***********************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <string>


namespace mirrage::version_info {
	extern const std::string name;
	extern const std::string hash;
	extern const std::string date;
	extern const std::string subject;
} // namespace mirrage::version_info
