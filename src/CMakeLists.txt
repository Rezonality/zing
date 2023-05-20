project(Zing
    LANGUAGES CXX C
    VERSION 0.5.0
)

find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)
find_path(ABLETON_LINK_INCLUDE_DIRS "ableton/Link.hpp")

set(ZING_SOURCE

    ${ZING_ROOT}/project.natvis

    ${ZING_ROOT}/src/wavetable/wavetable.cpp
    ${ZING_ROOT}/src/wavetable/wavetable.h

    # Audio
    ${ZING_ROOT}/src/audio/audio.cpp
    ${ZING_ROOT}/src/audio/audio_analysis.cpp
    ${ZING_ROOT}/include/zing/audio/audio.h
    ${ZING_ROOT}/include/zing/audio/audio_analysis_settings.h
    ${ZING_ROOT}/include/zing/audio/audio_device_settings.h
)

SET(ZEST_DIR ${ZING_ROOT}/libs/zing)
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

include_directories(${ZEST_DIR}/libs ${ZEST_DIR}/include)

target_compile_definitions(Zing
    PUBLIC
    NO_LIBSNDFILE
    _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)

target_link_libraries(Zing
PRIVATE
    tomlplusplus::tomlplusplus
PUBLIC
    Zest::Zest
    unofficial::concurrentqueue::concurrentqueue
    #unofficial::kissfft::kissfft
    fmt::fmt
    glm::glm
    Vulkan::Vulkan
    portaudio_static
    imgui::imgui
    date::date
    date::date-tz)

target_precompile_headers(Zing
  PRIVATE
    ${ZING_ROOT}/include/zing/audio/audio.h
)
set_source_files_properties (${SOUNDPIPE_SOURCE} PROPERTIES SKIP_PRECOMPILE_HEADERS 1)

target_include_directories(Zing
PRIVATE
    ${CMAKE_BINARY_DIR}
PUBLIC
    $<BUILD_INTERFACE:${ZING_ROOT}/libs>
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
    $<BUILD_INTERFACE:${ZING_ROOT}/include>
    $<BUILD_INTERFACE:${ZING_ROOT}/tomlplusplus/include>
    $<BUILD_INTERFACE:${ZEST_DIR}/libs>
    $<BUILD_INTERFACE:${ZEST_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Set locations for components
set_target_properties(Zing PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install the binary
install(TARGETS Zing
    EXPORT zing-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
)

# Install the include files
install(DIRECTORY ${ZING_ROOT}/include/zing
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

source_group ("zing" FILES ${ZING_SOURCE})
source_group ("soundpipe" FILES ${SOUNDPIPE_SOURCE})
source_group ("earlevel" FILES ${EARLEVEL_SOURCE})

