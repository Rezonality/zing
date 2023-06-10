project(Zing_App VERSION 0.1.0.0)

set(APP_NAME Zing_App)

# Set additional project information
set(COMPANY "Zing")
set(COPYRIGHT "Copyright (c) 2019-2023 Chris Maughan. All rights reserved.")
set(IDENTIFIER "com.zing")

set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR})

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/demo_common.cmake)

set(ZING_APP_SOURCE
    ${APP_ROOT}/main.cpp
    ${APP_ROOT}/demo.cpp
    ${APP_ROOT}/demo_draw_midi.cpp
    ${APP_ROOT}/demo_draw_analysis.cpp
    ${APP_ROOT}/demo.h
    ${APP_ROOT}/CMakeLists.txt
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
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_BINARY_DIR}
    )

target_link_libraries (${APP_NAME}
    PRIVATE
    Zing::Zing
    SDL2::SDL2main
    ${PLATFORM_LINKLIBS}
    )

target_precompile_headers(${APP_NAME}
  PRIVATE
    ${APP_ROOT}/pch.h
)
target_compile_definitions(${APP_NAME}
    PUBLIC
    NO_LIBSNDFILE
    _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)

if (WIN32)
    copy_existing_files(${PROJECT_NAME} "${RESOURCE_DEPLOY_FILES}" ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
endif()

if (APPLE)
    install(TARGETS ${DEMO_NAME}
        EXPORT zing-targets
        COMPONENT binaries
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS})
else()
    install(TARGETS ${DEMO_NAME}
        EXPORT zing-targets
        COMPONENT binaries
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/imgui
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
        )
endif()

source_group ("Source" FILES ${ZING_APP_SOURCE})
source_group ("SoundPipe" FILES ${SOUNDPIPE_SOURCE})

