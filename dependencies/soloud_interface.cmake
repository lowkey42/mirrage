cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(soloud)

find_package(Threads)

add_library(soloud STATIC 
	soloud/include/soloud_audiosource.h
	soloud/include/soloud_bassboostfilter.h
	soloud/include/soloud_biquadresonantfilter.h
	soloud/include/soloud_bus.h
	soloud/include/soloud_c.h
	soloud/include/soloud_dcremovalfilter.h
	soloud/include/soloud_echofilter.h
	soloud/include/soloud_error.h
	soloud/include/soloud_fader.h
	soloud/include/soloud_fftfilter.h
	soloud/include/soloud_fft.h
	soloud/include/soloud_file.h
	soloud/include/soloud_file_hack_off.h
	soloud/include/soloud_file_hack_on.h
	soloud/include/soloud_filter.h
	soloud/include/soloud_flangerfilter.h
	soloud/include/soloud.h
	soloud/include/soloud_internal.h
	soloud/include/soloud_lofifilter.h
	soloud/include/soloud_monotone.h
	soloud/include/soloud_openmpt.h
	soloud/include/soloud_queue.h
	soloud/include/soloud_robotizefilter.h
	soloud/include/soloud_sfxr.h
	soloud/include/soloud_speech.h
	soloud/include/soloud_tedsid.h
	soloud/include/soloud_thread.h
	soloud/include/soloud_vic.h
	soloud/include/soloud_vizsn.h
	soloud/include/soloud_waveshaperfilter.h
	soloud/include/soloud_wav.h
	soloud/include/soloud_wavstream.h

	soloud/src/core/soloud_audiosource.cpp
	soloud/src/core/soloud_core_getters.cpp
	soloud/src/core/soloud_fft.cpp
	soloud/src/core/soloud_bus.cpp
	soloud/src/core/soloud_core_setters.cpp
	soloud/src/core/soloud_fft_lut.cpp
	soloud/src/core/soloud_core_3d.cpp
	soloud/src/core/soloud_core_voicegroup.cpp
	soloud/src/core/soloud_file.cpp
	soloud/src/core/soloud_core_basicops.cpp
	soloud/src/core/soloud_core_voiceops.cpp
	soloud/src/core/soloud_filter.cpp
	soloud/src/core/soloud_core_faderops.cpp
	soloud/src/core/soloud.cpp
	soloud/src/core/soloud_queue.cpp
	soloud/src/core/soloud_core_filterops.cpp
	soloud/src/core/soloud_fader.cpp
	soloud/src/core/soloud_thread.cpp
	
	soloud/src/filter/soloud_bassboostfilter.cpp
	soloud/src/filter/soloud_flangerfilter.cpp
	soloud/src/filter/soloud_biquadresonantfilter.cpp
	soloud/src/filter/soloud_lofifilter.cpp
	soloud/src/filter/soloud_dcremovalfilter.cpp
	soloud/src/filter/soloud_robotizefilter.cpp
	soloud/src/filter/soloud_echofilter.cpp
	soloud/src/filter/soloud_waveshaperfilter.cpp
	soloud/src/filter/soloud_fftfilter.cpp
	
	soloud/src/audiosource/monotone/soloud_monotone.cpp
	soloud/src/audiosource/openmpt/soloud_openmpt.cpp
	soloud/src/audiosource/openmpt/soloud_openmpt_dll.c
	soloud/src/audiosource/sfxr/soloud_sfxr.cpp
	soloud/src/audiosource/speech/darray.cpp
	soloud/src/audiosource/speech/klatt.cpp
	soloud/src/audiosource/speech/resonator.cpp
	soloud/src/audiosource/speech/tts.cpp
	soloud/src/audiosource/speech/darray.h
	soloud/src/audiosource/speech/klatt.h
	soloud/src/audiosource/speech/resonator.h
	soloud/src/audiosource/speech/tts.h
	soloud/src/audiosource/speech/soloud_speech.cpp
	soloud/src/audiosource/tedsid/sid.cpp
	soloud/src/audiosource/tedsid/sid.h
	soloud/src/audiosource/tedsid/soloud_tedsid.cpp
	soloud/src/audiosource/tedsid/ted.cpp
	soloud/src/audiosource/tedsid/ted.h
	soloud/src/audiosource/vic/soloud_vic.cpp
	soloud/src/audiosource/vizsn/soloud_vizsn.cpp
	soloud/src/audiosource/wav/dr_flac.h
	soloud/src/audiosource/wav/dr_mp3.h
	soloud/src/audiosource/wav/soloud_wav.cpp
	soloud/src/audiosource/wav/stb_vorbis.c
	soloud/src/audiosource/wav/dr_impl.cpp
	soloud/src/audiosource/wav/dr_wav.h
	soloud/src/audiosource/wav/soloud_wavstream.cpp
	soloud/src/audiosource/wav/stb_vorbis.h

	soloud/src/backend/sdl2_static/soloud_sdl2_static.cpp
	soloud/src/backend/null/soloud_null.cpp
)
add_library(soloud::soloud ALIAS soloud)

target_link_libraries(soloud PUBLIC Threads::Threads mirrage::deps::SDL2)
target_compile_definitions(soloud PUBLIC WITH_NULL WITH_SDL2_STATIC)

target_include_directories(soloud SYSTEM PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/soloud/include>
	$<INSTALL_INTERFACE:include>)
	
install(TARGETS soloud EXPORT soloudTargets
	INCLUDES DESTINATION include
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

export(
	EXPORT soloudTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/soloudTargets.cmake"
)

install(
	EXPORT soloudTargets FILE soloudTargets.cmake
	DESTINATION lib/cmake
)
