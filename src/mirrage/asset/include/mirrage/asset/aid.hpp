/** GUUID for all assets used in the project *********************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/str_id.hpp>

#include <string>
#include <memory>

namespace lux {
namespace asset {

	using Asset_type = util::Str_id;

	/**
	 * Asset_type ':' Name; not case-sensitiv; e.g. "tex:Player/main"
	 */
	class AID {
		public:
			AID() : _type("gen") {}
			AID(std::string n);
			AID(Asset_type c, std::string n);

			bool operator==(const AID& o)const noexcept;
			bool operator!=(const AID& o)const noexcept;
			bool operator<(const AID& o)const noexcept;
			operator bool()const noexcept;

			auto str()const noexcept -> std::string;
			auto type()const noexcept{return _type;}
			auto name()const noexcept{return _name;}

		private:
			Asset_type _type;
			std::string _name;
	};

}
}

inline lux::asset::AID operator "" _aid(const char* str, std::size_t) {
	return lux::asset::AID(str);
}

namespace std {
	template <> struct hash<lux::asset::AID> {
		size_t operator()(const lux::asset::AID& aid)const noexcept {
			return 71*hash<lux::asset::Asset_type>()(aid.type())
			        + hash<string>()(aid.name());
		}
	};
}

