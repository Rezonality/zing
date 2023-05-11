project(Zing_App VERSION 0.1.0.0)

set(APP_NAME Zing_App)

# Set additional project information
set(COMPANY "Zing")
set(COPYRIGHT "Copyright (c) 2019-2023 Chris Maughan. All rights reserved.")
set(IDENTIFIER "com.nodegraph")

set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR})

find_package(portaudio CONFIG REQUIRED)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/demo_common.cmake)

SET(SOUNDPIPE_DIR ${CMAKE_CURRENT_LIST_DIR}/libs/soundpipe)
SET(SOUNDPIPE_EXTENSIONS_DIR ${CMAKE_CURRENT_LIST_DIR}/libs/soundpipe_extensions)

MESSAGE(INFO ${SOUNDPIPE_DIR})
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

SET(EARLEVEL_DIR ${CMAKE_CURRENT_LIST_DIR}/libs/earlevel)
set(EARLEVEL_SOURCE ${EARLEVEL_DIR}/el_wavetable_utils.cpp)

set(ZING_APP_SOURCE
    ${SOUNDPIPE_SOURCE}
    ${EARLEVEL_SOURCE}
    ${APP_ROOT}/main.cpp
    ${APP_ROOT}/demo.cpp
    ${APP_ROOT}/CMakeLists.txt
    ${APP_ROOT}/nodes/node_oscillator.cpp
    ${APP_ROOT}/nodes/node_oscillator.h
    ${APP_ROOT}/utils/wavetable.cpp
    ${APP_ROOT}/utils/wavetable.h
    )

set(RESOURCE_FOLDER ${CMAKE_CURRENT_LIST_DIR}/res)
add_project_meta(META_FILES_TO_INCLUDE)

add_executable (${APP_NAME}
    ${OS_BUNDLE}
    ${ZING_APP_SOURCE}
    ${META_FILES_TO_INCLUDE}
    )

target_include_directories(${APP_NAME}
    PRIVATE
    ${SOUNDPIPE_DIR}/h
    ${SOUNDPIPE_DIR}/lib/dr_wav
    ${SOUNDPIPE_DIR}/lib/faust
    ${SOUNDPIPE_DIR}/lib/inih
    ${SOUNDPIPE_DIR}/lib/openlpc
    ${SOUNDPIPE_DIR}/lib/kissfft
    ${SOUNDPIPE_DIR}/lib/spa
    ${SOUNDPIPE_DIR}/h
    ${SOUNDPIPE_EXTENSIONS_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_BINARY_DIR}
    )

target_link_libraries (${APP_NAME}
    PRIVATE
    Zing::Zing
    imgui::imgui
    SDL2::SDL2main
    portaudio_static
    ${PLATFORM_LINKLIBS}
    )

target_compile_definitions(${APP_NAME}
    PUBLIC
    NO_LIBSNDFILE
    _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)

add_dependencies(${APP_NAME} unittests)

if (WIN32)
    copy_existing_files(${PROJECT_NAME} "${RESOURCE_DEPLOY_FILES}" ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
endif()

if (APPLE)
    install(TARGETS ${DEMO_NAME}
        EXPORT nodegraph-targets
        COMPONENT binaries
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS})
else()
    install(TARGETS ${DEMO_NAME}
        EXPORT nodegraph-targets
        COMPONENT binaries
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/imgui
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
        )
endif()

source_group ("Source" FILES ${ZING_APP_SOURCE})
source_group ("SoundPipe" FILES ${SOUNDPIPE_SOURCE})

