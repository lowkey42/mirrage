#include <mirrage/translations.hpp>

#include <sf2/sf2.hpp>

#include <locale>


namespace mirrage {

	namespace {
		struct Language_info {
			std::vector<std::string> supported_languages;
			std::string              default_language;
		};
		sf2_structDef(Language_info, supported_languages, default_language);

		struct Language_cfg {
			std::string text_language;
		};
		sf2_structDef(Language_cfg, text_language);

		auto get_env_language()
		{
			auto locale = std::locale{""}.name();
			auto equals = locale.find('=');
			if(equals != std::string::npos)
				locale = locale.substr(equals + 1);

			auto lang = locale.substr(0, locale.find_first_of("._"));

			return lang;
		}
		void normalize_language(std::string& l) { util::to_lower_inplace(l); }

		template <class C, class T>
		auto contains(const C& container, const T& v)
		{
			return std::find(begin(container), end(container), v) != container.end();
		}
	} // namespace

	namespace asset {
		auto Loader<Localisation_data>::load(istream in) -> Localisation_data
		{
			auto r = Localisation_data{};

			auto on_error = [&](auto& msg, uint32_t row, uint32_t column) {
				LOG(plog::error) << "Error parsing JSON from " << in.aid().str() << " at " << row << ":"
				                 << column << ": " << msg;
			};

			sf2::JsonDeserializer reader{sf2::format::Json_reader{in, on_error}, on_error};
			reader.read_lambda([&](auto& category) {
				reader.read_value(r.categories[category]);
				return true;
			});

			return r;
		}
	} // namespace asset


	Translator::Translator(asset::Asset_manager& assets) : _assets(assets)
	{

		auto cfg = assets.load_maybe<Language_cfg>("cfg:language"_aid);

		auto language = cfg.process(get_env_language(), [](auto& c) { return c->text_language; });
		normalize_language(language);

		_language = Language_id{language};

		_reload();
	}
	Translator::~Translator() { _print_missing(); }

	auto Translator::supported_languages() const -> std::vector<Language_id>
	{
		return _assets.load<Language_info>("cfg:language_info"_aid)->supported_languages;
	}

	void Translator::_reload()
	{
		_print_missing();

		auto info = _assets.load<Language_info>("cfg:languages_info"_aid);

		if(!contains(info->supported_languages, _language)) {
			LOG(plog::info) << "Unsupported language: " << _language;
			_language = Language_id{info->default_language};
		}
		LOG(plog::info) << "Using text language: " << _language;

		_missing_categories.clear();
		_missing_translations.clear();
		_load(_language);

		auto entry_count = static_cast<std::size_t>(0);
		auto cat_count   = static_cast<std::size_t>(0);
		for(auto& f : _files) {
			for(auto& cat : f->categories) {
				cat_count++;
				entry_count += cat.second.size();
			}
		}
		LOG(plog::debug) << "Loaded " << entry_count << " translations in " << cat_count << " categories for "
		                 << _language;
	}
	void Translator::_load(const Language_id& lang)
	{
		auto loc_extension = "." + lang + ".json";

		_files.clear();

		for(auto& loc : _assets.list("loc"_strid)) {
			if(util::ends_with(loc.name(), loc_extension)) {
				_files.push_back(_assets.load<Localisation_data>(loc));
			}
		}
	}

	auto Translator::translate(const std::string& str) const -> const std::string&
	{
		return translate("", str);
	}

	auto Translator::translate(const Category_id& category, const std::string& str) const
	        -> const std::string&
	{
		for(auto& f : _files) {
			auto cat_iter = f->categories.find(category);

			if(cat_iter == f->categories.end())
				continue;

			auto iter = cat_iter->second.find(str);
			if(iter == cat_iter->second.end()) {
				if(_missing_translations.emplace(category, str).second) {
					LOG(plog::warning) << "Missing translation for language '" << _language << "' "
					                   << category << ": " << str;
				}
				return str;
			}

			return iter->second;
		}

		if(_missing_categories.emplace(category).second) {
			LOG(plog::warning) << "Missing translation category for language '" << _language
			                   << "': " << category;
		}
		return str;
	}

	void Translator::_print_missing() const
	{
		for(auto& category : _missing_categories) {
			LOG(plog::warning) << "Missing translation category for language '" << _language
			                   << "': " << category;
		}
		for(auto& e : _missing_translations) {
			auto&& category = e.first;
			auto&& str      = e.second;

			LOG(plog::warning) << "Missing translation for language '" << _language << "' " << category
			                   << ": " << str;
		}
	}
} // namespace mirrage
