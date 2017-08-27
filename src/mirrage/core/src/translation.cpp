#include <mirrage/translations.hpp>

#include <sf2/sf2.hpp>

#include <locale>


namespace mirrage {

	namespace {
		struct Language_info {
			std::vector<std::string> supported_languages;
			std::string default_language;
		};
		sf2_structDef(Language_info, supported_languages, default_language);

		struct Language_cfg {
			std::string text_language;
		};
		sf2_structDef(Language_cfg, text_language);

		auto get_env_language() {
			auto locale = std::locale{""}.name();
			auto lang = locale.substr(0, locale.find_first_of("._"));

			return lang;
		}
		void normalize_language(std::string& l) {
			util::to_lower_inplace(l);
		}

		template<class C, class T>
		auto contains(const C& container, const T& v) {
			return std::find(begin(container), end(container), v) != container.end();
		}
	}

	Translator::Translator(asset::Asset_manager& assets)
	    : _assets(assets) {

		auto cfg = assets.load_maybe<Language_cfg>("cfg:language"_aid);

		auto language = cfg.process(get_env_language(), [](auto& c){return c->text_language;});
		normalize_language(language);

		_language = Language_id{language};

		_reload();
	}
	Translator::~Translator() {
		_print_missing();
	}

	auto Translator::supported_languages()const -> std::vector<Language_id> {
		auto info = _assets.load<Language_info>("cfg:language_info"_aid);
		return info->supported_languages;
	}

	void Translator::_reload() {
		_print_missing();

		auto info = _assets.load<Language_info>("cfg:languages_info"_aid);

		if(!contains(info->supported_languages, _language)) {
			INFO("Unsupported language: "<<_language);
			_language = Language_id{info->default_language};
		}
		INFO("Using text language: "<<_language);

		_categories.clear();
		_missing_categories.clear();
		_missing_translations.clear();
		_load(_language);

		auto count = static_cast<std::size_t>(0);
		for(auto& cat : _categories) {
			count += cat.second.size();
		}
		DEBUG("Loaded "<<count<<" translations in "<<_categories.size()<<" categories for "<<_language);


		for(auto& wid : util::range_reverse(_loc_files_watchids)) {
			_assets.unwatch(wid);
		}
		_loc_files_watchids.clear();
		for(auto& loc : _assets.list("loc"_strid)) {
			_loc_files_watchids.emplace_back(_assets.watch(loc, [&](auto&) {
				DEBUG("Reload translations");
				this->_reload();
			}));
		}

	}
	void Translator::_load(const Language_id& lang) {
		auto loc_extension = "."+lang+".json";

		for(auto& loc : _assets.list("loc"_strid)) {
			if(util::ends_with(loc.name(), loc_extension)) {
				auto on_error = [&](auto& msg, uint32_t row, uint32_t column) {
					ERROR("Error parsing JSON from "<<loc.str()<<" at "<<row<<":"<<column<<": "<<msg);
				};

				_assets.load_raw(loc).process([&](auto& in) {
					sf2::JsonDeserializer reader{sf2::format::Json_reader{in, on_error}, on_error};
					reader.read_lambda([&](auto& category) {
						reader.read_value(_categories[category]);
						return true;
					});
				});
			}
		}
	}

	auto Translator::translate(const std::string& str)const -> const std::string& {
		return translate("", str);
	}

	auto Translator::translate(const Category_id& category, const std::string& str)const -> const std::string& {
		auto cat_iter = _categories.find(category);
		if(cat_iter==_categories.end()) {
			if(_missing_categories.emplace(category).second) {
				WARN("Missing translation category for language '"<<_language<<"': "<<category);
			}
			return str;
		}

		auto iter = cat_iter->second.find(str);
		if(iter==cat_iter->second.end()) {
			if(_missing_translations.emplace(category, str).second) {
				WARN("Missing translation for language '"<<_language<<"' "<<category<<": "<<str);
			}
			return str;
		}
		return iter->second;
	}

	void Translator::_print_missing()const {
		for(auto& category : _missing_categories) {
			WARN("Missing translation category for language '"<<_language<<"': "<<category);
		}
		for(auto& e : _missing_translations) {
			auto&& category = e.first;
			auto&& str = e.second;

			WARN("Missing translation for language '"<<_language<<"' "<<category<<": "<<str);
		}
	}
}
