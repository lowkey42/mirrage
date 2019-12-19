/** A collection of related sound effect *************************************
 *                                                                           *
 * Copyright (c) 2019 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/audio/audio_manager.hpp>

#include <mirrage/utils/random.hpp>
#include <mirrage/utils/random_vector.hpp>
#include <mirrage/utils/str_id.hpp>

#include <soloud_speech.h>

#include <unordered_map>
#include <variant>


namespace mirrage::audio {

	class Sound_bank {
	  public:
		auto get(util::Str_id, util::default_rand&) const -> Sample_ptr;

	  private:
		friend struct mirrage::asset::Loader<mirrage::audio::Sound_bank>;

		mutable std::unordered_map<util::Str_id, util::random_vector<Sample_ptr>> _effects;
	};

} // namespace mirrage::audio


namespace mirrage::asset {

	template <>
	struct Loader<mirrage::audio::Sound_bank> {
		static auto load(istream in) -> mirrage::audio::Sound_bank;
		static void save(ostream out, const mirrage::audio::Sound_bank& asset)
		{
			MIRRAGE_FAIL("not implemented");
		}
	};

} // namespace mirrage::asset
