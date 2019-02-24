#include <mirrage/asset/embedded_asset.hpp>

#include <mirrage/utils/log.hpp>

namespace mirrage::asset {

	auto Embedded_asset::_instances() noexcept -> std::vector<const Embedded_asset*>&
	{
		static std::vector<const Embedded_asset*> list;
		return list;
	}

	Embedded_asset::Embedded_asset(std::string name, gsl::span<const gsl::byte> data)
	  : _name(std::move(name)), _data(std::move(data))
	{
		_instances().emplace_back(this);
	}

} // namespace mirrage::asset
