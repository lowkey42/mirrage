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
		auto translate(const Category_id& category, const std::string& str) const -> const std::string&;
		// TODO: may need printf-style translate(...)


	  private:
		struct string_pair_hash {
			inline std::size_t operator()(const std::pair<std::string, std::string>& v) const {
				std::hash<std::string> h{};
				return h(v.first) * 31 + h(v.second);
			}
		};

		using Translation_table = std::unordered_map<std::string, std::string>;
		using Category_table    = std::unordered_map<Category_id, Translation_table>;

		using Missing_categories = std::unordered_set<std::string>;
		using Missing_translations =
		        std::unordered_set<std::pair<std::string, std::string>, string_pair_hash>;


		asset::Asset_manager& _assets;
		Language_id           _language;
		Category_table        _categories;
		std::vector<uint32_t> _loc_files_watchids;

		mutable Missing_categories   _missing_categories;
		mutable Missing_translations _missing_translations;

		void _reload();
		void _load(const Language_id& lang);
		void _print_missing() const;
	};
} // namespace mirrage
