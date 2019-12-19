#include <mirrage/audio/sound_bank.hpp>


namespace mirrage::audio {

	auto Sound_bank::get(util::Str_id name, util::default_rand& rng) const -> Sample_ptr
	{
		if(auto iter = _effects.find(name); iter != _effects.end()) {
			return iter->second.get_random(rng);
		} else
			return {};
	}

} // namespace mirrage::audio

namespace mirrage::asset {

	struct Sound_effect_config {
		bool                   speech = false;
		std::string            text;
		unsigned int           freq        = 1330;
		float                  speed       = 10.f;
		float                  declination = 0.5f;
		audio::Speech_waveform waveform    = audio::Speech_waveform::triangle;
	};
	void load(sf2::JsonDeserializer& s, Sound_effect_config& cfg)
	{
		if(s.reader.peek() == '{') {
			cfg.speech = true;
			s.read_virtual(sf2::vmember("text", cfg.text),
			               sf2::vmember("freq", cfg.freq),
			               sf2::vmember("speed", cfg.speed),
			               sf2::vmember("declination", cfg.declination),
			               sf2::vmember("waveform", cfg.waveform));

		} else {
			cfg.speech = false;
			s.read_value(cfg.text);
		}
	}

	auto Loader<mirrage::audio::Sound_bank>::load(istream in) -> mirrage::audio::Sound_bank
	{
		auto on_error = [&](auto& msg, uint32_t row, uint32_t column) {
			LOG(plog::error) << "Error parsing JSON from " << in.aid().str() << " at " << row << ":" << column
			                 << ": " << msg;
		};

		auto cfg = std::unordered_map<std::string, std::vector<Sound_effect_config>>{};
		sf2::JsonDeserializer{sf2::format::Json_reader{in, on_error}, on_error}.read_value(cfg);

		auto bank = mirrage::audio::Sound_bank{};
		for(auto&& [key, val] : cfg) {
			auto& vec = bank._effects[util::Str_id(key)].raw_vector();
			vec.reserve(val.size());
			for(auto& v : val) {
				if(v.speech) {
					vec.emplace_back(
					        audio::make_speech_sample(v.text, v.freq, v.speed, v.declination, v.waveform));
				} else {
					vec.emplace_back(in.manager().load<audio::Sample>(AID(v.text)));
				}
			}
		}

		return bank;
	}

} // namespace mirrage::asset
