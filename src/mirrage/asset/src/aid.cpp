#include <mirrage/asset/aid.hpp>

#include <mirrage/utils/string_utils.hpp>
#include <mirrage/utils/log.hpp>

namespace mirrage {
namespace asset {

	AID::AID(Asset_type t, std::string n)
		: _type(t), _name(std::move(n)) {
	}

	AID::AID(std::string n) {
		auto r = util::split(n, ":");
		util::to_lower_inplace(r.first);
		util::to_lower_inplace(r.second);
		_type = util::Str_id(r.first);
		_name = std::move(r.second);
	}

	std::string AID::str()const noexcept {
		return _type.str()+":"+_name;
	}

	bool AID::operator==(const AID& o)const noexcept {
		return _type==o._type && _name==o._name;
	}
	bool AID::operator!=(const AID& o)const noexcept {
		return !(*this==o);
	}
	bool AID::operator<(const AID& o)const noexcept {
		return _type<o._type || _name<o._name;
	}
	AID::operator bool()const noexcept {
		return !_name.empty();
	}

}
}
