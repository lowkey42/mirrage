#include <mirrage/audio/audio_manager.hpp>

#include <soloud_wav.h>
#include <soloud_wavstream.h>


namespace mirrage::audio {


	auto make_speech_sample(const std::string& str,
	                        unsigned int       aBaseFrequency,
	                        float              aBaseSpeed,
	                        float              aBaseDeclination,
	                        Speech_waveform    aBaseWaveform) -> Sample_ptr
	{
		auto s = std::make_unique<SoLoud::Speech>();
		s->setParams(aBaseFrequency, aBaseSpeed, aBaseDeclination, static_cast<int>(aBaseWaveform));
		s->setText(str.c_str());
		return asset::make_ready_asset(asset::AID("speech"_strid, "_GEN_"),
		                               std::unique_ptr<SoLoud::AudioSource>(std::move(s)));
	}

	int          Audio_file::eof() { return _stream.eof() ? 1 : 0; }
	unsigned int Audio_file::read(unsigned char* aDst, unsigned int aBytes)
	{
		_stream.read(reinterpret_cast<std::istream::char_type*>(aDst), static_cast<std::streamsize>(aBytes));
		return static_cast<unsigned int>(_stream.gcount());
	}
	unsigned int Audio_file::length() { return static_cast<unsigned int>(_stream.length()); }
	void         Audio_file::seek(int aOffset) { _stream.seekg(aOffset); }
	unsigned int Audio_file::pos() { return static_cast<unsigned int>(_stream.tellg()); }


	Audio_manager::Audio_manager(asset::Asset_manager& assets, bool enable) : _assets(assets)
	{
		auto maybe_settings = assets.load_maybe<Audio_settings>("cfg:audio"_aid);
		if(maybe_settings.is_nothing()) {
			if(!settings(Audio_settings{})) {
				MIRRAGE_FAIL("Invalid audio settings");
			}
		} else {
			_settings = maybe_settings.get_or_throw();
		}

		_soloud.init(SoLoud::Soloud::CLIP_ROUNDOFF,
		             enable ? SoLoud::Soloud::SDL2 : SoLoud::Soloud::NULLDRIVER,
		             SoLoud::Soloud::AUTO,
		             SoLoud::Soloud::AUTO,
		             _settings->channels);

		if(!settings(*_settings)) {
			MIRRAGE_FAIL("Couldn't apply audio settings");
		}
	}

	bool Audio_manager::settings(const Audio_settings& new_settings)
	{
		_assets.save<Audio_settings>("cfg:audio"_aid, new_settings);
		_settings = _assets.load<Audio_settings>("cfg:audio"_aid);
		backend().setGlobalVolume(_settings->volume);
		return true;
	}

	void Audio_manager::update(util::Time dt)
	{
		_time += dt.value();
		util::erase_if(_sample_keep_alive, [&](auto& v) { return std::get<0>(v) < _time; });
	}

	void Audio_manager::play(Sample_ptr sample, float volume)
	{
		backend().playClocked(time(), **sample, volume * _settings->ui_volume, 0.f);
	}

	void Audio_manager::keep_alive(Sample_ptr&& sample, float time)
	{
		_sample_keep_alive.emplace_back(time, std::move(sample));
	}


	Music_container::Music_container(Audio_manager& audio, Sample_ptr sample, float fade_time)
	  : _audio(&audio)
	{
		play(std::move(sample), fade_time);
	}
	Music_container::~Music_container()
	{
		if(_music && _music_handle != 0) {
			_audio->backend().fadeVolume(_music_handle, 0, 0.5f);
			_audio->backend().scheduleStop(_music_handle, 0.5f);
			_audio->keep_alive(std::move(_music), 0.5f);
		}
	}

	void Music_container::pause()
	{
		if(_music && _music_handle != 0 && !_paused) {
			_audio->backend().fadeVolume(_music_handle, 0, 0.5f);
			_audio->backend().schedulePause(_music_handle, 0.5f);
			_paused = true;
		}
	}
	void Music_container::unpause()
	{
		if(_music && _music_handle != 0 && _paused) {
			_audio->backend().setPause(_music_handle, false);
			_audio->backend().fadeVolume(_music_handle, _audio->settings().music_volume, 0.5f);
			_paused = false;
		}
	}

	void Music_container::play(Sample_ptr sample, float fade_time)
	{
		stop(fade_time);

		_music        = std::move(sample);
		_music_handle = _audio->backend().playBackground(**_music, 0.f, 0.f);
		_audio->backend().setLooping(_music_handle, true);
		_audio->backend().fadeVolume(_music_handle, _audio->settings().music_volume, fade_time);
	}
	void Music_container::stop(float fade_time)
	{
		if(_music && _music_handle != 0) {
			_audio->backend().fadeVolume(_music_handle, 0, fade_time);
			_audio->backend().scheduleStop(_music_handle, fade_time);
			_audio->keep_alive(std::move(_music), fade_time);
			_music_handle = 0;
		}
	}

} // namespace mirrage::audio


namespace mirrage::asset {

	/// Wrapper that also holds onto the original audio file
	/// Required because WavStream only accepts a non-owning pointer
	class OwningWavStream : public SoLoud::WavStream {
	  public:
		OwningWavStream(istream&& in) : _file(std::move(in)) { loadFile(&_file); }

	  private:
		mirrage::audio::Audio_file _file;
	};

	auto Loader<mirrage::audio::Sample>::load(istream in) -> mirrage::audio::Sample
	{
		switch(in.aid().type()) {
			case "wav"_strid: {
				auto s    = std::make_unique<SoLoud::Wav>();
				auto file = mirrage::audio::Audio_file(std::move(in));
				s->loadFile(&file);
				return s;
			}
			case "wav_stream"_strid: return std::make_unique<OwningWavStream>(std::move(in));

			default: MIRRAGE_FAIL("Unsupported AID-type for audio sample: " << in.aid().str());
		}
	}
} // namespace mirrage::asset
