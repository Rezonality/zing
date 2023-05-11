project(Zing
    LANGUAGES CXX C
    VERSION 0.5.0
)

find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)

set(ZING_SOURCE
    ${ZING_ROOT}/src/setting.cpp

    ${ZING_ROOT}/src/time/timer.cpp
    ${ZING_ROOT}/include/zing/time/timer.h

    ${ZING_ROOT}/project.natvis

    ${ZING_ROOT}/utils/wavetable.cpp
    ${ZING_ROOT}/utils/wavetable.h

    ${ZING_ROOT}/include/zing/imgui_glm.h
    ${ZING_ROOT}/include/zing/math_utils.h

    ${ZING_ROOT}/src/string/string_utils.cpp
    ${ZING_ROOT}/include/zing/string/string_utils.h
    ${ZING_ROOT}/include/zing/string/murmur_hash.h

)

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

target_link_libraries(Zing
PRIVATE
    tomlplusplus::tomlplusplus
PUBLIC
    fmt::fmt-header-only
    glm::glm
    Vulkan::Vulkan
    portaudio_static
    imgui::imgui
    date::date
    date::date-tz)

target_include_directories(Zing
PRIVATE
    ${CMAKE_BINARY_DIR}
PUBLIC
    ${SOUNDPIPE_DIR}/h
    ${SOUNDPIPE_DIR}/lib/dr_wav
    ${SOUNDPIPE_DIR}/lib/faust
    ${SOUNDPIPE_DIR}/lib/inih
    ${SOUNDPIPE_DIR}/lib/openlpc
    ${SOUNDPIPE_DIR}/lib/kissfft
    ${SOUNDPIPE_DIR}/lib/spa
    ${SOUNDPIPE_DIR}/h
    ${SOUNDPIPE_EXTENSIONS_DIR}
    $<BUILD_INTERFACE:${ZING_ROOT}/include>
    $<BUILD_INTERFACE:${ZING_ROOT}/tomlplusplus/include>
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
source_group ("vulkan" FILES ${ZING_VULKAN_SOURCE})

