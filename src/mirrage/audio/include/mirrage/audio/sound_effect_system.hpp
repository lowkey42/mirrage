/** ECS system to manage sound effect ****************************************
 *                                                                           *
 * Copyright (c) 2019 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/audio/audio_manager.hpp>

#include <mirrage/ecs/ecs.hpp>
#include <mirrage/utils/random.hpp>
#include <mirrage/utils/units.hpp>


namespace mirrage::audio {

	class Sound_effect_system {
	  public:
		Sound_effect_system(ecs::Entity_manager&, Audio_manager&);

		void pause();
		void unpause();

		void play(Sample_ptr, float volume = 1.f, ecs::Entity_handle = {});

		/// like play but with different InaudibleBehavior and using Audio_settings::dialog_volume
		void play_dialog(Sample_ptr, float volume = 1.f, ecs::Entity_handle = {});

		void update(util::Time dt);

	  private:
		ecs::Entity_manager&   _ecs;
		Audio_manager&         _audio;
		SoLoud::Bus            _bus;
		unsigned int           _bus_handle;
		util::default_rand     _rand;
		util::maybe<glm::vec3> _last_listener_position;

		auto _play(Sample_ptr, float volume, ecs::Entity_handle) -> unsigned int;
	};

} // namespace mirrage::audio
