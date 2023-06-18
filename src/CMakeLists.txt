project(Zing
    LANGUAGES CXX C
    VERSION 0.5.0
)

find_package(glm CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)
find_path(ABLETON_LINK_INCLUDE_DIRS "ableton/Link.hpp")

set(ZING_SF2_SOURCE
    #${ZING_ROOT}/src/sf2/file.cpp
)

set(ZING_REMIDI_SOURCE
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/alsa.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/coreaudio.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/dummy.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/emscripten.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/emscripten_api.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/jack.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/midi_api.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/raw_alsa.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/raw_alsa_helpers.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/semaphore.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/winmm.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/detail/winuwp.hpp

   ${ZING_ROOT}/libs/remidi/include/libremidi/message.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/reader.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/writer.hpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/libremidi.hpp

   ${ZING_ROOT}/libs/remidi/include/libremidi/libremidi.cpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/reader.cpp
   ${ZING_ROOT}/libs/remidi/include/libremidi/writer.cpp
)

set(ZING_AUDIO_SOURCE
    ${ZING_ROOT}/src/pch.cpp
    ${ZING_ROOT}/src/audio/audio.cpp
    ${ZING_ROOT}/src/audio/audio_analysis.cpp
    ${ZING_ROOT}/src/audio/audio_samples.cpp
    ${ZING_ROOT}/src/audio/midi.cpp

    # Audio
    ${ZING_ROOT}/include/zing/audio/audio.h
    ${ZING_ROOT}/include/zing/audio/audio_samples.h
    ${ZING_ROOT}/include/zing/audio/audio_analysis_settings.h
    ${ZING_ROOT}/include/zing/audio/audio_device_settings.h
    ${ZING_ROOT}/include/zing/audio/midi.h
)

set(ZING_WAVETABLE_SOURCE
    ${ZING_ROOT}/src/wavetable/wavetable.cpp
    ${ZING_ROOT}/src/wavetable/wavetable.h
)

set(ZING_SOURCE
    ${ZING_SF2_SOURCE}
    ${ZING_AUDIO_SOURCE}
    ${ZING_REMIDI_SOURCE}
    ${ZING_WAVETABLE_SOURCE}
    ${ZING_ROOT}/project.natvis
)

SET(SOUNDPIPE_DIR ${ZING_ROOT}/libs/soundpipe)
SET(SOUNDPIPE_DIR ${ZING_ROOT}/libs/soundpipe)
SET(SOUNDPIPE_EXTENSIONS_DIR ${ZING_ROOT}/libs/soundpipe_extensions)

file(GLOB_RECURSE SOUNDPIPE_SOURCE "${SOUNDPIPE_DIR}/modules/*.c")
list (REMOVE_ITEM SOUNDPIPE_SOURCE "${SOUNDPIPE_DIR}/modules/diskin.c")

LIST(APPEND SOUNDPIPE_SOURCE
    ${SOUNDPIPE_DIR}/lib/dr_wav/dr_wav.c
    ${SOUNDPIPE_DIR}/lib/spa/spa.c
    ${SOUNDPIPE_DIR}/lib/fft/fft.c
    ${SOUNDPIPE_DIR}/lib/kissfft/kiss_fft.c
    ${SOUNDPIPE_DIR}/lib/kissfft/kiss_fftr.c
    ${SOUNDPIPE_DIR}/lib/openlpc/openlpc.c
    ${SOUNDPIPE_DIR}/lib/inih/ini.c
    ${SOUNDPIPE_EXTENSIONS_DIR}/modules/oscmorph2d.c)

if (WIN32)
    # Sound pipe plays fast and loose with float/double conversions and other things.
    # To be fair, this is probably its inherited Csound code.
    # Certainly on windows there are a few compile warnings which I'm ignoring for now
    set_source_files_properties(${SOUNDPIPE_SOURCE} PROPERTIES COMPILE_FLAGS "/wd4244 /wd4101 /wd4334 /wd4305 /wd4005 /wd4273 /wd4018 /wd4267 /wd4129")
endif()

SET(EARLEVEL_DIR ${ZING_ROOT}/libs/earlevel)
set(EARLEVEL_SOURCE ${EARLEVEL_DIR}/el_wavetable_utils.cpp)

set(ZING_SOURCE
    ${ZING_SOURCE}
    ${EARLEVEL_SOURCE}
    ${SOUNDPIPE_SOURCE}
    ${ZING_ROOT}/CMakeLists.txt
)

list(APPEND TEST_SOURCES
#    ${ZING_DIR}/src/model/zing.test.cpp
)

add_library(Zing STATIC ${ZING_SOURCE})
add_library(Zing::Zing ALIAS Zing)

target_compile_definitions(Zing
    PUBLIC
    NO_LIBSNDFILE
    _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)

if(WIN32)
target_compile_definitions(Zing
    PUBLIC
    LIBREMIDI_WINMM
    LIBREMIDI_JACK
)
endif()

target_link_libraries(Zing
PUBLIC
    Zest::Zest
PRIVATE
    portaudio_static)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(Zing PUBLIC atomic)
endif()

target_precompile_headers(Zing
  PRIVATE
    ${ZING_ROOT}/include/zing/pch.h
)
set_source_files_properties (${SOUNDPIPE_SOURCE} PROPERTIES SKIP_PRECOMPILE_HEADERS 1)

target_include_directories(Zing
PRIVATE
    ${CMAKE_BINARY_DIR}
PUBLIC
    $<BUILD_INTERFACE:${ZING_ROOT}/libs/remidi/include>
    $<BUILD_INTERFACE:${ZING_ROOT}/libs>
    $<BUILD_INTERFACE:${ZING_ROOT}/include>
    $<BUILD_INTERFACE:${ZING_ROOT}/tomlplusplus/include>
    $<BUILD_INTERFACE:${ABLETON_LINK_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/h>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/lib/dr_wav>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/lib/faust>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/lib/inih>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/lib/openlpc>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/lib/kissfft>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/lib/spa>
    $<BUILD_INTERFACE:${SOUNDPIPE_DIR}/h>
    $<BUILD_INTERFACE:${SOUNDPIPE_EXTENSIONS_DIR}>
)

source_group ("zing/audio" FILES ${ZING_AUDIO_SOURCE})
source_group ("zing/wavetable" FILES ${ZING_WAVETABLE_SOURCE})
source_group ("zing/sf2" FILES ${ZING_SF2_SOURCE})
source_group ("zing/remidi" FILES ${ZING_REMIDI_SOURCE})
source_group ("soundpipe" FILES ${SOUNDPIPE_SOURCE})
source_group ("earlevel" FILES ${EARLEVEL_SOURCE})

