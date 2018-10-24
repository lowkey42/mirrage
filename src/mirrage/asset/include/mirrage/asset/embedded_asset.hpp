/** interface to register embedded asset archives ****************************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <gsl/gsl>
#include <string>
#include <vector>

namespace mirrage::asset {

	class Embedded_asset {
	  public:
		Embedded_asset(std::string name, gsl::span<const gsl::byte> data);

		auto name() const noexcept -> auto& { return _name; }
		auto data() const noexcept { return _data; }

		static auto instances() noexcept -> gsl::span<const Embedded_asset*> { return _instances(); }

	  private:
		const std::string          _name;
		gsl::span<const gsl::byte> _data;

		static auto _instances() noexcept -> std::vector<const Embedded_asset*>&;
	};

} // namespace mirrage::asset
