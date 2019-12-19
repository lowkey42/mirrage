/** initialization & management of audio backend *****************************
 *                                                                           *
 * Copyright (c) 2019 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/asset/stream.hpp>
#include <mirrage/utils/units.hpp>

#include <soloud.h>
#include <soloud_file.h>
#include <soloud_speech.h>

#include <sf2/sf2.hpp>


namespace mirrage::audio {

	using Sample     = std::unique_ptr<SoLoud::AudioSource>;
	using Sample_ptr = asset::Ptr<Sample>;

	enum class Speech_waveform { saw, triangle, sin, square, pulse, noise, warble };
	sf2_enumDef(Speech_waveform, saw, triangle, sin, square, pulse, noise, warble);

	extern auto make_speech_sample(const std::string& str,
	                               unsigned int       aBaseFrequency   = 1330,
	                               float              aBaseSpeed       = 10.0f,
	                               float              aBaseDeclination = 0.5f,
	                               Speech_waveform aBaseWaveform = Speech_waveform::triangle) -> Sample_ptr;

	class Audio_file : public SoLoud::File {
	  public:
		Audio_file(asset::istream s) : _stream(std::move(s)) {}

		virtual int          eof() override;
		virtual unsigned int read(unsigned char* aDst, unsigned int aBytes) override;
		virtual unsigned int length() override;
		virtual void         seek(int aOffset) override;
		virtual unsigned int pos() override;

	  private:
		asset::istream _stream;
	};

	struct Audio_settings {
		int channels = 2; //< 1)mono, 2)stereo 4)quad 6)5.1 8)7.1

		float volume          = 1.f;
		float music_volume    = 0.5f;
		float ui_volume       = 1.f;
		float gameplay_volume = 0.8f;
		float dialog_volume   = 1.f;
	};
	sf2_structDef(Audio_settings, channels, volume, music_volume, ui_volume, gameplay_volume, dialog_volume);

	class Audio_manager {
	  public:
		Audio_manager(asset::Asset_manager& assets, bool enable = true);

		auto settings() const noexcept -> const Audio_settings& { return *_settings; }
		bool settings(const Audio_settings&);

		void update(util::Time dt);

		auto& backend() { return _soloud; }
		auto  time() const { return _time; }

		/// mainly for ui sound effects
		void play(Sample_ptr, float volume = 1.f);

		void keep_alive(Sample_ptr&&, float time);

	  private:
		asset::Asset_manager&                     _assets;
		asset::Ptr<Audio_settings>                _settings;
		SoLoud::Soloud                            _soloud;
		double                                    _time = 0;
		std::vector<std::pair<float, Sample_ptr>> _sample_keep_alive;
	};

	class Music_container {
	  public:
		Music_container(Audio_manager&, Sample_ptr, float fade_time = 0.5f);
		Music_container(Music_container&&) = default;
		Music_container& operator=(Music_container&&) = default;
		~Music_container();

		void pause();
		void unpause();

		void play(Sample_ptr, float fade_time = 0.5f);
		void stop(float fade_time = 0.5f);

	  private:
		Audio_manager* _audio;
		Sample_ptr     _music;
		unsigned int   _music_handle;
		bool           _paused = false;
	};

} // namespace mirrage::audio


namespace mirrage::asset {

	template <>
	struct Loader<mirrage::audio::Sample> {
		static auto load(istream in) -> mirrage::audio::Sample;
		static void save(ostream out, const mirrage::audio::Sample& asset)
		{
			MIRRAGE_FAIL("not implemented");
		}
		static void reload(istream, mirrage::audio::Sample&)
		{
			LOG(plog::warning) << "Hot-Reloading is not supported for audio samples";
		}
	};

} // namespace mirrage::asset
