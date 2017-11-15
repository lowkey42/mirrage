/** Provides translations of user-facing strings *****************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/asset_manager.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>


namespace mirrage {

	using Category_id = std::string;
	using Language_id = std::string;

	struct Localisation_data {
		using Translation_table = std::unordered_map<std::string, std::string>;
		using Category_table    = std::unordered_map<Category_id, Translation_table>;

		Category_table categories;
	};


	class Translator {
	  public:
		Translator(asset::Asset_manager&);
		~Translator();

		void language(const Language_id& l) {
			auto changed = _language != l;
			_language    = l;
			if(changed) {
				_reload();
			}
		}
		auto language() const { return _language; }
		auto supported_languages() const -> std::vector<Language_id>;

		auto translate(const std::string& str) const -> const std::string&;
		auto translate(const Category_id& category, const std::string& str) const
		        -> const std::string&;
		// TODO: may need printf-style translate(...)


	  private:
		struct string_pair_hash {
			inline std::size_t operator()(const std::pair<std::string, std::string>& v) const {
				std::hash<std::string> h{};
				return h(v.first) * 31 + h(v.second);
			}
		};

		using Missing_categories = std::unordered_set<std::string>;
		using Missing_translations =
		        std::unordered_set<std::pair<std::string, std::string>, string_pair_hash>;


		asset::Asset_manager&                      _assets;
		Language_id                                _language;
		std::vector<asset::Ptr<Localisation_data>> _files;

		mutable Missing_categories   _missing_categories;
		mutable Missing_translations _missing_translations;

		void _reload();
		void _load(const Language_id& lang);
		void _print_missing() const;
	};
} // namespace mirrage

namespace mirrage::asset {
	/**
	 * Specialize this template for each asset-type
	 * Instances should be lightweight
	 * Implementations should NEVER return nullptr
	 */
	template <>
	struct Loader<Localisation_data> {
		static auto              load(istream in) -> std::shared_ptr<Localisation_data>;
		[[noreturn]] static void save(ostream, const Localisation_data&) {
			MIRRAGE_FAIL("store<Localisation_data>(...) not supported!");
		}
	};

} // namespace mirrage::asset
